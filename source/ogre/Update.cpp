#include "pch.h"
#include "Defines.h"
#include "OgreGame.h"
#include "FollowCamera.h"
#include "../road/Road.h"
#include "../vdrift/game.h"
#include "../paged-geom/PagedGeometry.h"
#include "../ogre/common/MaterialGen/MaterialFactory.h"
#include "../oisb/OISBSystem.h"

#include <OgreParticleSystem.h>
#include <OgreManualObject.h>
#include <OgreMaterialManager.h>
#include "common/Gui_Def.h"
#include "common/MultiList2.h"
using namespace Ogre;


//---------------------------------------------------------------------------------------------------------------
//  Frame Start
//---------------------------------------------------------------------------------------------------------------

void App::UpdThr()
{
	while (!mShutDown)
	{
		///  step Game  **
		//  separate thread
		if (pSet->multi_thr == 1 && !bLoading)
		{
			bool ret = pGame->OneLoop();

			//if (!pGame->pause && mFCam)
			//	mFCam->update(pGame->framerate/**/);
			//if (ndSky)  ///o-
			//	ndSky->setPosition(GetCamera()->getPosition());

			if (!ret)
				mShutDown = true;
		#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
			Sleep(0);  // par
		#endif
		}
		/*else
		{	//  2nd test --
			if (road) {  //--
				if (grass)  grass->update();
				if (trees)  trees->update();  }
			Sleep(0);  // par
		}/**/
	}
}

//---------------------------------------------------------------------------------------------------------------

bool App::frameStart(Real time)
{
	if (bGuiReinit)  // after language change from combo
	{	bGuiReinit = false;

		mGUI->destroyWidgets(vwGui);  bnQuit=0;mWndOpts=0;  //todo: rest too..
		InitGui();
		bWindowResized = true;
		mWndTabs->setIndexSelected(6);  // switch back to view tab
	}

	if (bWindowResized)
	{	bWindowResized = false;
		ResizeOptWnd();
		SizeGUI();
		updTrkListDim();
		bSizeHUD = true;
	}
		
	///  sort trk list
	if (trkMList && trkMList->mSortColumnIndex != trkMList->mSortColumnIndexOld
		|| trkMList->mSortUp != trkMList->mSortUpOld)
	{
		trkMList->mSortColumnIndexOld = trkMList->mSortColumnIndex;
		trkMList->mSortUpOld = trkMList->mSortUp;

		pSet->tracks_sort = trkMList->mSortColumnIndex;  // to set
		pSet->tracks_sortup = trkMList->mSortUp;
		TrackListUpd(false);
	}

	if (bLoading)
	{
		NewGameDoLoad();
		return true;
	}
	else 
	{
		bool bFirstFrame = (carModels.size()>0 && carModels.front()->bGetStPos) ? true : false;
		
		if (isFocGui && mWndTabs->getIndexSelected() == 7)
			UpdateInputBars();
		
		//  keys dn/up - trklist, carlist
		#define isKey(a)  mKeyboard->isKeyDown(OIS::a)
		static float dirU = 0.f,dirD = 0.f;
		if (isFocGui)
		{	if (isKey(KC_UP)  ||isKey(KC_NUMPAD8))	dirD += time;  else
			if (isKey(KC_DOWN)||isKey(KC_NUMPAD2))	dirU += time;  else
			{	dirU = 0.f;  dirD = 0.f;  }
			int d = ctrl ? 4 : 1;
			if (dirU > 0.0f) {  carLNext( d);  trkLNext( d);  rplLNext( d);  dirU = -0.12f;  }
			if (dirD > 0.0f) {  carLNext(-d);  trkLNext(-d);  rplLNext(-d);  dirD = -0.12f;  }
		}
		
		//  replay forward,backward keys
		if (bRplPlay)
		{
			isFocRpl = ctrl;
			bool le = isKey(KC_LBRACKET), ri = isKey(KC_RBRACKET), ctrlN = ctrl && (le || ri);
			int ta = ((le || bRplBack) ? -2 : 0) + ((ri || bRplFwd) ? 2 : 0);
			if (ta)
			{	double tadd = ta;
				tadd *= (shift ? 0.2 : 1) * (ctrlN ? 4 : 1) * (alt ? 8 : 1);  // multipliers
				if (!bRplPause)  tadd -= 1;  // play compensate
				double t = pGame->timer.GetReplayTime(), len = replay.GetTimeLength();
				t += tadd * time;  // add
				if (t < 0.0)  t += len;  // cycle
				if (t > len)  t -= len;
				pGame->timer.SetReplayTime(t);
			}
		}

		if (!pGame)
			return false;
		pGame->pause = bRplPlay ? (bRplPause || isFocGui) : isFocGui;


		// input
		OISB::System::getSingleton().process(time);

		///  step Game  *******
		//  single thread, sim on draw
		bool ret = true;
		if (pSet->multi_thr != 1)
		{
			ret = pGame->OneLoop();
			if (!ret)  mShutDown = true;
			updatePoses(time);  //pGame->framerate
		}
		

		//  multi thread
		if (pSet->multi_thr == 1)
		{
			/// ???? ---------
			static QTimer gtim;
			gtim.update();
			double dt = gtim.dt;
			
			// Update cameras for all cars
			for (std::vector<CarModel*>::iterator it=carModels.begin(); it!=carModels.end(); it++)
				if ((*it)->fCam)
					(*it)->fCam->update(pGame->framerate);
					
			updatePoses(time);  //pGame->framerate
		}
		
		// align checkpoint arrow
		// move in front of camera
		if (pSet->check_arrow && arrowNode && !bRplPlay)
		{
			Vector3 camPos = carModels.front()->fCam->mCamera->getPosition();
			Vector3 dir = carModels.front()->fCam->mCamera->getDirection();
			dir.normalise();
			Vector3 up = carModels.front()->fCam->mCamera->getUp();
			up.normalise();
			Vector3 arrowPos = camPos + 10.0f * dir + 3.5f*up;
			arrowNode->setPosition(arrowPos);
			
			// animate
			if (bFirstFrame) // 1st frame: dont animate
				arrowAnimCur = arrowAnimEnd;
			else
				arrowAnimCur = Quaternion::Slerp(time*5, arrowAnimStart, arrowAnimEnd, true);
			arrowRotNode->setOrientation(arrowAnimCur);
			
			// look down -y a bit so we can see the arrow better
			arrowRotNode->pitch(Degree(-20), SceneNode::TS_LOCAL); 
		}

		//  update all cube maps
		for (std::vector<CarModel*>::iterator it=carModels.begin(); it!=carModels.end(); it++)
		if ((*it)->eType != CarModel::CT_GHOST && (*it)->pReflect)
			(*it)->pReflect->Update();

		//  trees
		//if (pSet->mult_thr != 2)
		if (road) {
			if (grass)  grass->update();
			if (trees)  trees->update();  }

		//  road upd lods
		if (road)
		{
			road->RebuildRoadInt();

			//  more than 1 in pre viewport, each frame
			if (pSet->local_players == 1)
			{
				if (roadUpCnt <= 0)
				{
					roadUpCnt = 15;  //par upd, time..
					road->UpdLodVis(pSet->road_dist);
				}
				roadUpCnt--;/**/
			}
		}

		//**  bullet bebug draw
		if (dbgdraw)  {
			dbgdraw->setDebugMode(pSet->bltDebug ? /*255*/1: 0);
			dbgdraw->step();  }

		///  terrain mtr from blend maps
		// now in CarModel::Update
		//UpdWhTerMtr(pCar);
		
		// stop rain/snow when paused
		if (pr && pr2 && pGame)
		{
			if (pGame->pause)
				{	 pr->setSpeedFactor(0.f);	 pr2->setSpeedFactor(0.f);	}
			else{	 pr->setSpeedFactor(1.f);	 pr2->setSpeedFactor(1.f);	}
		}
		
		materialFactory->update();
		
		bFirstRenderFrame = false;
		
		return ret;
	}
}
bool App::frameEnd(Real time)
{
	return true;
}


//  newPoses - Get new car pos from game
//---------------------------------------------------------------------------------------------------------------
void App::newPoses()
{
	if (!pGame)  return;
	if (pGame->cars.size() == 0)  return;
	double rplTime = pGame->timer.GetReplayTime();
	double lapTime = pGame->timer.GetPlayerTime();

	// Iterate through all car models and get new pos info
	int iCarNum = 0;  CarModel* carM0 = 0;
	std::vector<CarModel*>::iterator carMIt = carModels.begin();
	std::vector<PosInfo>::iterator newPosIt = newPosInfos.begin();
	while (carMIt != carModels.end())
	{
		CarModel* carM = *carMIt;  if (iCarNum==0)  carM0 = carM;
		CAR* pCar = carM->pCar;
		PosInfo posInfo;
		bool bGhost = carM->eType == CarModel::CT_GHOST;
		
		//  local data  car,wheels
		MATHVECTOR <float,3> pos, whPos[4];
		QUATERNION <float> rot, whRot[4];  //?double


		///-----------------------------------------------------------------------
		//  play  get data from replay
		///-----------------------------------------------------------------------
		if (bGhost)
		{
			ReplayFrame frame;
			bool ok = ghplay.GetFrame(lapTime, &frame, 0);
			//  car
			pos = frame.pos;  rot = frame.rot;
			//  wheels
			for (int w=0; w < 4; ++w)
			{
				whPos[w] = frame.whPos[w];  whRot[w] = frame.whRot[w];
				posInfo.whVel[w] = frame.whVel[w];
				posInfo.whSlide[w] = frame.slide[w];  posInfo.whSqueal[w] = frame.squeal[w];
				posInfo.whR[w] = replay.header.whR[iCarNum][w];//
				posInfo.whTerMtr[w] = frame.whTerMtr[w];  posInfo.whRoadMtr[w] = frame.whRoadMtr[w];
				posInfo.fboost = frame.fboost;
			}
			ghostFrame = frame;
		}
		else if (bRplPlay)
		{
			//  time  from start
			bool ok = replay.GetFrame(rplTime, &fr, iCarNum);
				if (!ok)	pGame->timer.RestartReplay();
			
			//  car
			pos = fr.pos;  rot = fr.rot;
			//  wheels
			for (int w=0; w < 4; ++w)
			{
				whPos[w] = fr.whPos[w];  whRot[w] = fr.whRot[w];
				posInfo.whVel[w] = fr.whVel[w];
				posInfo.whSlide[w] = fr.slide[w];  posInfo.whSqueal[w] = fr.squeal[w];
				posInfo.whR[w] = replay.header.whR[iCarNum][w];//
				posInfo.whTerMtr[w] = fr.whTerMtr[w];  posInfo.whRoadMtr[w] = fr.whRoadMtr[w];
				posInfo.fboost = fr.fboost;
			}
		}
		else
		//  get data from vdrift
		//-----------------------------------------------------------------------
		if (pCar)
		{
			pos = pCar->dynamics.GetPosition();
			rot = pCar->dynamics.GetOrientation();
			
			for (int w=0; w < 4; ++w)
			{	WHEEL_POSITION wp = WHEEL_POSITION(w);
				whPos[w] = pCar->dynamics.GetWheelPosition(wp);
				whRot[w] = pCar->dynamics.GetWheelOrientation(wp);
				//float wR = pCar->GetTireRadius(wp);
				posInfo.whVel[w] = pCar->dynamics.GetWheelVelocity(wp).Magnitude();
				posInfo.whSlide[w] = -1.f;  posInfo.whSqueal[w] = pCar->GetTireSquealAmount(wp, &posInfo.whSlide[w]);
				posInfo.whR[w] = pCar->GetTireRadius(wp);//
				posInfo.whTerMtr[w] = carM->whTerMtr[w];  posInfo.whRoadMtr[w] = carM->whRoadMtr[w];
			}
			posInfo.fboost = pCar->dynamics.boostVal;
		}
		

		//  transform axes, vdrift to ogre  car & wheels
		//-----------------------------------------------------------------------

		posInfo.pos = Vector3(pos[0],pos[2],-pos[1]);
		Quaternion q(rot[0],rot[1],rot[2],rot[3]), q1;
		Radian rad;  Vector3 axi;  q.ToAngleAxis(rad, axi);
		q1.FromAngleAxis(-rad,Vector3(axi.z,-axi.x,-axi.y));  posInfo.rot = q1 * qFixCar;
		Vector3 vcx,vcz;  q1.ToAxes(vcx,posInfo.carY,vcz);

		if (!isnan(whPos[0][0]))
		for (int w=0; w < 4; w++)
		{
			posInfo.whPos[w] = Vector3(whPos[w][0],whPos[w][2],-whPos[w][1]);
			Quaternion q(whRot[w][0],whRot[w][1],whRot[w][2],whRot[w][3]), q1;
			Radian rad;  Vector3 axi;  q.ToAngleAxis(rad, axi);
			q1.FromAngleAxis(-rad,Vector3(axi.z,-axi.x,-axi.y));  posInfo.whRot[w] = q1 * qFixWh;
		}
		posInfo.bNew = true;
		

		///  sound listener  - - - - -
		if (!bGhost)
		{
			if (pGame->sound.Enabled())  // TODO: set from camera, for each player? ..
			{
				pGame->sound.SetListener(
					MATHVECTOR <float,3> (pos[0], pos[1], pos[2]),
					QUATERNION <float>(),
					//QUATERNION <float> (rot.x(), rot.y(), rot.z(), rot.w()),
					MATHVECTOR <float,3>());
			}
			bool incar = true;
			if (pCar)
			{
				std::list <SOUNDSOURCE *> soundlist;
				pCar->GetEngineSoundList(soundlist);
				for (std::list <SOUNDSOURCE *>::iterator s = soundlist.begin(); s != soundlist.end(); s++)
					(*s)->Set3DEffects(!incar);
			}
		}

		///-----------------------------------------------------------------------
		//  record  save data for replay
		///-----------------------------------------------------------------------
		if (pSet->rpl_rec && !pGame->pause && !bGhost && pCar)
		{
			//static int ii = 0;
			//if (ii++ >= 0)	// 1 half game framerate
			{	//ii = 0;
				ReplayFrame fr;
				fr.time = rplTime;  //  time  from start
				fr.pos = pos;  fr.rot = rot;  //  car
				//  wheels
				for (int w=0; w < 4; w++)
				{	fr.whPos[w] = whPos[w];  fr.whRot[w] = whRot[w];

					WHEEL_POSITION wp = WHEEL_POSITION(w);
					const TRACKSURFACE* surface = pCar->dynamics.GetWheelContact(wp).GetSurfacePtr();
					fr.surfType[w] = !surface ? TRACKSURFACE::NONE : surface->type;
					//  squeal
					fr.slide[w] = -1.f;  fr.squeal[w] = pCar->GetTireSquealAmount(wp, &fr.slide[w]);
					fr.whVel[w] = pCar->dynamics.GetWheelVelocity(wp).Magnitude();
					//  susp
					fr.suspVel[w] = pCar->dynamics.GetSuspension(wp).GetVelocity();
					fr.suspDisp[w] = pCar->dynamics.GetSuspension(wp).GetDisplacementPercent();
					//replay.header.whR[w] = pCar->GetTireRadius(wp);//
					fr.whTerMtr[w] = carM->whTerMtr[w];  fr.whRoadMtr[w] = carM->whRoadMtr[w];
				}
				//  hud
				fr.vel = pCar->GetSpeedometer();  fr.rpm = pCar->GetEngineRPM();
				fr.gear = pCar->GetGear();  fr.clutch = pCar->GetClutch();
				fr.throttle = pCar->dynamics.GetEngine().GetThrottle();
				fr.steer = pCar->GetLastSteer();
				fr.fboost = pCar->dynamics.doBoost;
				//  eng snd
				fr.posEngn = pCar->dynamics.GetEnginePosition();
				fr.speed = pCar->GetSpeed();
				fr.dynVel = pCar->dynamics.GetVelocity().Magnitude();
				
				replay.AddFrame(fr, iCarNum);  // rec replay
				if (iCarNum==0)  /// rec ghost lap
				{
					fr.time = lapTime;
					ghost.AddFrame(fr, 0);
				}
				
				if (valRplName2)  // recorded info
				{
					int size = replay.GetNumFrames() * sizeof(ReplayFrame);
					sprintf(s, "%5.2f", float(size)/1000000.f);
					String ss = String( TR("#{RplRecTime}: ")) + GetTimeString(replay.GetTimeLength()) + TR("   #{RplSize}: ") + s + TR(" #{UnitMB}");
					valRplName2->setCaption(ss);
				}
			}
		}
		if (bRplPlay && valRplName2)  valRplName2->setCaption("");
		///-----------------------------------------------------------------------
		

		//  chekpoints, lap start
		//-----------------------------------------------------------------------
		if (bRplPlay || bGhost || !sc.ter)   // dont check when replay play
			carM->bWrongChk = false;
		else
		{
			// checkpoint arrow
			if (pSet->check_arrow && !bRplPlay && arrowNode && road && road->mChks.size()>0)
			{
				// set animation start to old orientation
				arrowAnimStart = arrowAnimCur;
				
				// game start: no animation
				bool noAnim = carM->iNumChks == 0;
				
				// get vector from camera to checkpoint
				Vector3 chkPos = road->mChks[std::max(0, std::min((int)road->mChks.size()-1, carM->iNextChk))].pos;
					
				// workaround for last checkpoint
				if (carM->iNumChks == road->mChks.size())
				{
					// point arrow to start position
					chkPos = carM->vStartPos;
				}
				
				//const Vector3& playerPos = carM->fCam->mCamera->getPosition();
				const Vector3& playerPos = carM->pMainNode->getPosition();
				Vector3 dir = chkPos - playerPos;
				dir[1] = 0; // only x and z rotation
				Quaternion quat = Vector3::UNIT_Z.getRotationTo(-dir); // convert to quaternion

				const bool valid = !quat.isNaN();
				if (valid) {
					if (noAnim) arrowAnimStart = quat;
					arrowAnimEnd = quat;
				
					// set arrow color (wrong direction: red arrow)
					// calc angle towards cam
					Real angle = (arrowAnimCur.zAxis().dotProduct(carM->fCam->mCamera->getOrientation().zAxis())+1)/2.0f;
					// set color in material
					MaterialPtr arrowMat = MaterialManager::getSingleton().getByName("Arrow");
					if (arrowMat->getTechnique(0)->getPass(1)->hasFragmentProgram())
					{
						GpuProgramParametersSharedPtr fparams = arrowMat->getTechnique(0)->getPass(1)->getFragmentProgramParameters();
						// green: 0.0 1.0 0.0     0.0 0.4 0.0
						// red:   1.0 0.0 0.0     0.4 0.0 0.0
						Vector3 col1 = angle * Vector3(0.0, 1.0, 0.0) + (1-angle) * Vector3(1.0, 0.0, 0.0);
						Vector3 col2 = angle * Vector3(0.0, 0.4, 0.0) + (1-angle) * Vector3(0.4, 0.0, 0.0);
						fparams->setNamedConstant("color1", col1);
						fparams->setNamedConstant("color2", col2);
					}
				}
			}
			
			if (carM->bGetStPos)  // first pos is at start
			{	carM->bGetStPos = false;
				carM->matStPos.makeInverseTransform(posInfo.pos, Vector3::UNIT_SCALE, posInfo.rot);
				carM->ResetChecks();
			}
			if (road && !carM->bGetStPos)
			{
				//  start/finish box dist
				Vector4 carP(posInfo.pos.x,posInfo.pos.y,posInfo.pos.z,1);
				carM->vStDist = carM0->matStPos * carP;  // start pos from 1st car always
				carM->bInSt = abs(carM->vStDist.x) < road->vStBoxDim.x && 
					abs(carM->vStDist.y) < road->vStBoxDim.y && 
					abs(carM->vStDist.z) < road->vStBoxDim.z;
							
				carM->iInChk = -1;  carM->bWrongChk = false;
				int ncs = road->mChks.size();
				if (ncs > 0)
				{	if (carM->bInSt && carM->iNumChks == ncs && carM->iCurChk != -1)  // finish
					{
						bool best = pGame->timer.Lap(iCarNum, 0,0, true, pSet->trackreverse);  //pGame->cartimerids[pCar] ?

						if (!pSet->rpl_bestonly || best)  ///  new best lap, save ghost
						if (iCarNum==0 && pSet->rpl_rec)  // for many, only 1st-
						{
							ghost.SaveFile(GetGhostFile());
							ghplay.CopyFrom(ghost);
						}
						ghost.Clear();
						
						carM->ResetChecks();
						//  restore boost fuel, each lap
						if (pSet->boost_type == 1 && carM->pCar)
							carM->pCar->dynamics.boostFuel = gfBoostFuelStart;

						///  winner places  for local players > 1
						if (carM->iWonPlace == 0 && pGame->timer.GetCurrentLap(iCarNum) >= pSet->num_laps)
							carM->iWonPlace = carIdWin++;
					}
					for (int i=0; i < ncs; ++i)
					{
						const CheckSphere& cs = road->mChks[i];
						Real d2 = posInfo.pos.squaredDistance(cs.pos);
						if (d2 < cs.r2)  // car in checkpoint
						{
							carM->iInChk = i;
							//  next check
							if (i == carM->iNextChk && carM->iNumChks < ncs)
							{
								carM->iCurChk = i;  carM->iNumChks++;
								int ii = (pSet->trackreverse ? -1 : 1) * road->iDir;
								carM->iNextChk = (carM->iCurChk + ii + ncs) % ncs;
								//  save car pos and rot
								carM->pCar->SavePosAtCheck();
							}
							else
							if (carM->iInChk != carM->iCurChk)
								carM->bWrongChk = true;
							break;
						}
				}	}
		}	}

		(*newPosIt) = posInfo;
		carMIt++;  newPosIt++;  iCarNum++;  // next
	}
}


//  updatePoses - Set car pos for Ogre nodes, update particles, trails
//---------------------------------------------------------------------------------------------------------------
void App::updatePoses(float time)
{	
	//  Update all carmodels with their newPosInfo
	int i=0;
	std::vector<CarModel*>::iterator carIt = carModels.begin();
	std::vector<PosInfo>::iterator newPosIt = newPosInfos.begin();
	while (carIt != carModels.end())
	{
		CarModel* carM = *carIt;
		if (!carM)  return;
		PosInfo newPosInfo = *newPosIt;
		
		//  hide ghost when empty
		bool bGhost = carM->eType == CarModel::CT_GHOST,
			bGhostVis = (ghplay.GetNumFrames() > 0) && pSet->rpl_ghost;
		if (bGhost)
		{
			carM->setVisible(bGhostVis);
			
			//  hide ghost car when close to player car (only when not transparent)
			if (!pSet->rpl_alpha)
			{
				CarModel* playerCar = carModels.front();
				float distance = carM->pMainNode->getPosition().squaredDistance(playerCar->pMainNode->getPosition());
				if (distance < 16.f)
					carM->setVisible(false);
			}
		}
		
		carM->Update(newPosInfo, time);
		
		//  nick text 3d pos
		//projectPoint()
		
		//  pos on minimap  x,y = -1..1
		//if (!bGhost)
		{	float xp =(-newPosInfo.pos[2] - minX)*scX*2-1,
				  yp =-(newPosInfo.pos[0] - minY)*scY*2+1;
			newPosInfos[i].miniPos = Vector2(xp,yp);
			
			if (vNdPos[i])
				if (bGhost && !bGhostVis)  vNdPos[i]->setPosition(-100,0,0);  //hide
				else if (pSet->mini_zoomed)  vNdPos[i]->setPosition(0,0,0);
				else					vNdPos[i]->setPosition(xp,yp,0);
		}
		carIt++;  newPosIt++;  i++;
	}
	
	///  Replay info
	if (bRplPlay && pGame->cars.size() > 0)
	{
		double pos = pGame->timer.GetPlayerTime();
		float len = replay.GetTimeLength();
		if (valRplPerc){  sprintf(s, "%4.1f %%", pos/len * 100.f);  valRplPerc->setCaption(s);  }
		if (valRplCur)  valRplCur->setCaption(GetTimeString(pos));
		if (valRplLen)  valRplLen->setCaption(GetTimeString(len));

		if (slRplPos)
		{	int v = pos/len * res;  slRplPos->setScrollPosition(v);  }
	}	
}


//  Update HUD rotated elems
//---------------------------------------------------------------------------------------------------------------
void App::UpdHUDRot(int carId, CarModel* pCarM, float vel, float rpm, bool miniOnly)
{
	/// TODO: rpm vel needle angles,aspect are wrong [all from the last car when bloom is on (any effects)], hud vals are ok
	//if (!pCarM || carId == -1)  return;
	//pCarM = carModels[carId];
	// todo:  poses when mini rotated or zoomed ..

    const float rsc = -180.f/6000.f, rmin = 0.f;  //rmp
    float angrmp = rpm*rsc + rmin;
    float vsc = pSet->show_mph ? -180.f/100.f : -180.f/160.f, vmin = 0.f;  //vel
    float angvel = abs(vel)*vsc + vmin;
    float angrot = 0.f;  int i=0;
	if (pCarM)	angrot = pCarM->angCarY;

	//*H*/LogO(String("caR: ") + toStr(carId) + /*/" " + toStr(pCarM) +*/ "  vel " + toStr(vel) + "  rpm " + toStr(rpm) + "  a " + toStr(angrot));
    float sx = 1.4f, sy = sx*asp;  // *par len
    float psx = 2.1f * pSet->size_minimap, psy = psx;  // *par len

    const static Real tc[4][2] = {{0,1}, {1,1}, {0,0}, {1,0}};  // defaults, no rot
    const static Real tp[4][2] = {{-1,-1}, {1,-1}, {-1,1}, {1,1}};
    const static float d2r = PI_d/180.f;
    const static Real ang[4] = {0.f,90.f,270.f,180.f};

    float rx[4],ry[4], vx[4],vy[4], px[4],py[4], cx[4],cy[4];  // rpm,vel, pos,crc
    for (int i=0; i<4; ++i)  // 4 verts, each+90deg
    {
		float ia = 45.f + ang[i];  //i*90.f;
		if (!miniOnly)
		{	float r = -(angrmp + ia) * d2r;		rx[i] = sx*cosf(r);  ry[i] =-sy*sinf(r);
			float v = -(angvel + ia) * d2r;		vx[i] = sx*cosf(v);  vy[i] =-sy*sinf(v);
		}
		float p = -(angrot + ia) * d2r;		float cp = cosf(p), sp = sinf(p);

		if (pSet->mini_rotated && pSet->mini_zoomed)
			{  px[i] = psx*tp[i][0];  py[i] = psy*tp[i][1];  }
		else{  px[i] = psx*cp*1.4f;     py[i] =-psy*sp*1.4f;  }

		float z = pSet->mini_rotated ? 0.70f/pSet->zoom_minimap : 0.5f/pSet->zoom_minimap;
		if (!pSet->mini_rotated)
			{  cx[i] = tp[i][0]*z;  cy[i] = tp[i][1]*z-1.f;  }
		else{  cx[i] =       cp*z;  cy[i] =      -sp*z-1.f;  }
    }
    
    //  rpm needle
    if (!miniOnly)
    {
		if (mrpm)  {	mrpm->beginUpdate(0);
			for (int p=0;p<4;++p)  {  mrpm->position(rx[p],ry[p], 0);  mrpm->textureCoord(tc[p][0], tc[p][1]);  }	mrpm->end();  }

		//  vel needle
		if (mvel)  {	mvel->beginUpdate(0);
			for (int p=0;p<4;++p)  {  mvel->position(vx[p],vy[p], 0);  mvel->textureCoord(tc[p][0], tc[p][1]);  }	mvel->end();  }
	}
		
	//  minimap car pos-es
	int c = carId;
	//for (int c=0; c < pSet->local_players; ++c)
	if (vMoPos[c])  {  vMoPos[c]->beginUpdate(0);
		for (int p=0;p<4;++p)  {  vMoPos[c]->position(px[p],py[p], 0);
			vMoPos[c]->textureCoord(tc[p][0], tc[p][1]);	if (pCarM)  vMoPos[c]->colour(pCarM->color);  }
		vMoPos[c]->end();  }

	
	//  minimap circle/rect
	if (miniC && pSet->trackmap)
	{	const Vector2& v = newPosInfos[carId].miniPos;
		float xc = (v.x+1.f)*0.5f, yc = (v.y+1.f)*0.5f;
		miniC->beginUpdate(0);
		if (!pSet->mini_zoomed)
			for (int p=0;p<4;++p)  {  miniC->position(tp[p][0],tp[p][1], 0);
				miniC->textureCoord(tc[p][0], tc[p][1]);  miniC->colour(tc[p][0],tc[p][1], 0);  }
		else
			for (int p=0;p<4;++p)  {  miniC->position(tp[p][0],tp[p][1], 0);
				miniC->textureCoord(cx[p]+xc, -cy[p]-yc);  miniC->colour(tc[p][0],tc[p][1], 1);  }
		miniC->end();  }
}

//  util
Vector2 App::projectPoint(Viewport* vp, const Vector3& pos)
{
	Camera* cam = vp->getCamera();
	Vector3 pos2D = cam->getProjectionMatrix() * (cam->getViewMatrix() * pos);

	Real x =       ((pos2D.x * 0.5f) + 0.5f);
	Real y = 1.f - ((pos2D.y * 0.5f) + 0.5f);

	return Vector2(x * vp->getWidth(), y * vp->getHeight());
}
