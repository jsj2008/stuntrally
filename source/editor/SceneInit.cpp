#include "stdafx.h"
#include "OgreApp.h"
#include "../road/Road.h"


//  Create Scene
//-------------------------------------------------------------------------------------
void App::createScene()
{
	//  camera
	asp = float(mWindow->getWidth())/float(mWindow->getHeight());
	mCamera->setFarClipDistance(pSet->view_distance*1.1f);
	mCamera->setNearClipDistance(0.1f);

	//  cam pos from last set
	mCameraT->setPosition(Vector3(pSet->cam_x,pSet->cam_y,pSet->cam_z));
	mCameraT->setDirection(Vector3(pSet->cam_dx,pSet->cam_dy,pSet->cam_dz).normalisedCopy());
	//mCamera->pitch(Degree(-73));  //mCamera->yaw(Degree(45));
	mViewport->setVisibilityMask(255);  // hide prv cam rect

	//  tex fil
	MaterialManager::getSingleton().setDefaultTextureFiltering(TFO_ANISOTROPIC);
	MaterialManager::getSingleton().setDefaultAnisotropy(pSet->anisotropy);

	//  gui
	InitGui();
	TerCircleInit();

	if (pSet->autostart)
		LoadTrack();
	else
	{	bGuiFocus = true;  UpdVisGui();	}
}


//---------------------------------------------------------------------------------------------------------------
///  Load Track
//---------------------------------------------------------------------------------------------------------------
void App::UpdTrees()
{
	if (!pSet->bTrees)
	{
		if (grass) {  delete grass->getPageLoader();  delete grass;  grass=0;   }
		if (trees) {  delete trees->getPageLoader();  delete trees;  trees=0;   }
	}else
		CreateTrees();
}

void App::NewCommon()
{
	//  destroy all
	if (ndSky)
		mSceneMgr->destroySceneNode(ndSky);

	if (grass) {  delete grass->getPageLoader();  delete grass;  grass=0;   }
	if (trees) {  delete trees->getPageLoader();  delete trees;  trees=0;   }

	//mSceneMgr->destroyAllStaticGeometry();

	terrain = 0;
	if (mTerrainGroup)
		mTerrainGroup->removeAllTerrains();

	if (resTrk != "")  mRoot->removeResourceLocation(resTrk);
		resTrk = TrkDir() + "objects";
	mRoot->addResourceLocation(resTrk, "FileSystem");
}
//---------------------------------------------------------------------------------------------------------------
void App::LoadTrack()
{
	eTrkEvent = TE_Load;
	Status("Loading...", 0.3,0.6,1.0);
}
void App::LoadTrackEv()
{
	NewCommon();

	if (road)
	{	road->Destroy();  delete road;  road = 0;  }

	// load scene
	sc.ter = true;
	sc.LoadXml(TrkDir()+"scene.xml");
	LoadSurf(pSet->track);
	UpdWndTitle();

	bNewHmap = false;/**/
	CreateTerrain();
	if (pSet->bTrees)
		CreateTrees();

	//  road ~
	road = new SplineRoad();
	road->Setup("sphere.mesh", 1.4f*pSet->road_sphr, terrain, mSceneMgr, mCamera);
	road->LoadFile(TrkDir()+"road.xml");
	UpdPSSMMaterials();


	//  updates after load
	//--------------------------
	ReadTrkStats();
	SetGuiFromXmls();  ///
	
	Rnd2TexSetup();
	UpdVisGui();
	LoadStartPos();

	Status("Loaded", 0.5,0.7,1.0);
}


///  Update Track
//---------------------------------------------------------------------------------------------------------------
void App::UpdateTrack()
{
	eTrkEvent = TE_Update;
	Status("Updating...",0.2,1.0,0.5);
}
void App::UpdateTrackEv()
{
	NewCommon();
	
	CreateTerrain(bNewHmap,true);/**/
	if (pSet->bTrees)
		CreateTrees();

	//  road ~
	road->mTerrain = terrain;
	road->RebuildRoad(true);
	UpdPSSMMaterials();

	Rnd2TexSetup();

	Status("Updated",0.5,1.0,0.7);
}


void App::UpdWndTitle()
{
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	HWND hwnd = 0;  // update wnd title
	mWindow->getCustomAttribute("WINDOW", (void*)&hwnd); 
	SetWindowText(hwnd, (String("SR Editor  track: ") + pSet->track).c_str());
#endif
	// TODO: Window title for linux
}


///  Save Terrain
//---------------------------------------------------------------------------------------------------------------
void App::SaveTrack()
{
	eTrkEvent = TE_Save;
	Status("Saving...", 1,0.4,0.1);
}
void App::SaveTrackEv()
{	
	if (terrain)
	{	float *fHmap = terrain->getHeightData();
		int size = sc.td.iVertsX * sc.td.iVertsY * sizeof(float);

		String file = TrkDir()+"heightmap.f32";
		ofstream of;
		of.open(file.c_str(), ios_base::binary);
		of.write((const char*)fHmap, size);
		of.close();
	}
	if (road)
		road->SaveFile(TrkDir()+"road.xml");

	sc.SaveXml(TrkDir()+"scene.xml");
	SaveSurf(pSet->track);

	SaveGrassDens();
	SaveStartPos();
	Status("Saved", 1,0.6,0.2);
}


///  Ter Circle mesh   o
//-------------------------------------------------------------------------------------
const int divs = 90;
const Real aAdd = 2 * 2*PI / divs, dTc = 2.f/(divs+1) *4;
static Real fTcos[divs+4], fTsin[divs+4];


void App::TerCircleInit()
{
	moTerC = mSceneMgr->createManualObject();
	moTerC->setDynamic(true);
	moTerC->setCastShadows(false);

	moTerC->estimateVertexCount(divs+2);
	moTerC->estimateIndexCount(divs+2);
	moTerC->begin("circle_deform", RenderOperation::OT_TRIANGLE_STRIP);

	for (int d = 0; d < divs+2; ++d)
	{
		Real a = d/2 * aAdd;	fTcos[d] = cosf(a);  fTsin[d] = sinf(a);
		Real r = (d % 2 == 0) ? 1.f : 0.9f;
		Real x = r * fTcos[d], z = r * fTsin[d];
		moTerC->position(x,0,z);  //moTerC->normal(0,1,0);
		moTerC->textureCoord(d/2*dTc, d%2);
	}
	moTerC->end();
 
	AxisAlignedBox aabInf;	aabInf.setInfinite();
	moTerC->setBoundingBox(aabInf);  // always visible
	moTerC->setRenderQueueGroup(RENDER_QUEUE_OVERLAY - 1);
	moTerC->setVisibilityFlags(2);  // hide on minimap
	ndTerC = mSceneMgr->getRootSceneNode()->createChildSceneNode(Vector3(0,0,0));
	ndTerC->attachObject(moTerC);  ndTerC->setVisible(false);
}


void App::TerCircleUpd()
{
	if (!moTerC || !terrain || !road)  return;

	bool edTer = bEdit() && (edMode == ED_Deform || edMode == ED_Smooth) && road->bHitTer;
	ndTerC->setVisible(edTer);
	if (!edTer)  return;
	
	Real rbr = mBrSize[curBr] * 0.5f * sc.td.fTriangleSize * 0.8f/*?par*/;

	static ED_MODE edOld = ED_ALL;
	if (edOld != edMode)
	{	edOld = edMode;
		moTerC->setMaterialName(0, edMode == ED_Deform ? "circle_deform" : "circle_smooth");  }

	moTerC->beginUpdate(0);
	for (int d = 0; d < divs+2; ++d)
	{
		Real a = d/2 * aAdd;
		Real r = ((d % 2 == 0) ? 1.0f : 0.95f) * rbr;
		Real x = r * fTcos[d], z = r * fTsin[d];
		Vector3 p(x,0,z);  p += road->posHit;
		p.y = terrain->getHeightAtWorldPosition(p) + 0.3f;
		moTerC->position(p);  //moTerC->normal(0,1,0);
		moTerC->textureCoord(d/2*dTc, d%2);
	}
	moTerC->end();
}