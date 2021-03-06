#include "pch.h"
#include "Defines.h"
#include "OgreApp.h"
#include "../road/Road.h"
#include <fstream>
#include "../ogre/common/Gui_Def.h"
using namespace MyGUI;
using namespace Ogre;


///  Gui Events

//  [Sky]
//-----------------------------------------------------------------------------------------------------------

void App::comboSky(ComboBoxPtr cmb, size_t val)  // sky materials
{
	String s = cmb->getItemNameAt(val);
	sc.skyMtr = s;  UpdateTrack();
}

void App::comboRain1(ComboBoxPtr cmb, size_t val)  // rain types
{
	String s = cmb->getItemNameAt(val);
	sc.rainName = s;
}
void App::comboRain2(ComboBoxPtr cmb, size_t val)
{
	String s = cmb->getItemNameAt(val);
	sc.rain2Name = s;
}

void App::slRain1Rate(SL)  // rain rates
{
	float v = 6000.f * val/res;		sc.rainEmit = v;
	if (valRain1Rate){	Fmt(s, "%4.0f", v);	valRain1Rate->setCaption(s);  }	UpdSun();
}
void App::slRain2Rate(SL)
{
	float v = 6000.f * val/res;		sc.rain2Emit = v;
	if (valRain2Rate){	Fmt(s, "%4.0f", v);	valRain2Rate->setCaption(s);  }	UpdSun();
}

void App::slSunPitch(SL)  // sun pitch, yaw
{
	float v = 90.f * val/res;	sc.ldPitch = v;
	if (valSunPitch){	Fmt(s, "%4.1f", v);	valSunPitch->setCaption(s);  }	UpdSun();
}
void App::slSunYaw(SL)
{
	float v = -180.f + 360.f * val/res;  sc.ldYaw = v;
	if (valSunYaw){	Fmt(s, "%4.1f", v);	valSunYaw->setCaption(s);  }  UpdSun();
}
void App::slFogStart(SL)  // fog start, end
{
	float v = 2000.f * powf(val/res, 2.f);		sc.fogStart = v;  UpdFog();
	if (valFogStart){	Fmt(s, "%3.0f", v);	valFogStart->setCaption(s);  }
}
void App::slFogEnd(SL)
{
	float v = 2000.f * powf(val/res, 2.f);		sc.fogEnd = v;    UpdFog();
	if (valFogEnd){		Fmt(s, "%3.0f", v);	valFogEnd->setCaption(s);  }
}

void App::chkFogDisable(WP wp)  // chk fog disable
{
	ChkEv(bFog);  UpdFog();
}
void App::editFogClr(Edit* ed)  // edit fog clr
{
	Vector3 c = s2v(ed->getCaption());  sc.fogClr = c;  UpdFog();
}
void App::editLiAmb(Edit* ed)  // edit light clrs
{
	Vector3 c = s2v(ed->getCaption());	sc.lAmb = c;  UpdSun();
}
void App::editLiDiff(Edit* ed)
{
	Vector3 c = s2v(ed->getCaption());	sc.lDiff = c;  UpdSun();
}
void App::editLiSpec(Edit* ed)
{
	Vector3 c = s2v(ed->getCaption());	sc.lSpec = c;  UpdSun();
}


///  [Terrain]
//-----------------------------------------------------------------------------------------------------------

///  Change terrain texture layer, update values
//
void App::tabTerLayer(TabPtr wp, size_t id)
{
	idTerLay = id;  // help vars
	bTerLay = id < sc.td.ciNumLay;
	float scale = 0.f;
	TerLayer* lay = &sc.td.layerRoad;
	noBlendUpd = true;

	//if (tabsTerLayers->getItemSelected()->getCaption().asUTF8() == "Road")
	cmbTexDiff->setVisible(bTerLay);  cmbTexNorm->setVisible(bTerLay);
	chkTerLay->setVisible(bTerLay);   chkTexNormAuto->setVisible(bTerLay);
	imgTexDiff->setVisible(bTerLay);  edTerLScale->setVisible(bTerLay);
	if (bTerLay)
	{	if (id >= sc.td.ciNumLay)  return;
		lay = &sc.td.layersAll[id];

		chkTerLay->setStateSelected(lay->on);
		cmbTexDiff->setIndexSelected( cmbTexDiff->findItemIndexWith(lay->texFile) );
		cmbTexNorm->setIndexSelected( cmbTexNorm->findItemIndexWith(lay->texNorm) );

		//  auto norm check  if  norm = tex + _nh
		String sTex,sNorm, sExt;
		StringUtil::splitBaseFilename(lay->texFile,sTex,sExt);
		StringUtil::splitBaseFilename(lay->texNorm,sNorm,sExt);
		bool bAuto = sTex + "_nh" == sNorm;
		chkTexNormAuto->setStateSelected(bAuto);
		//  tex image
	    imgTexDiff->setImageTexture(sTex + "_prv.png");
	    scale = lay->tiling;

		//  Ter Blendmap
		ScrollBar* sl;  size_t v;
		Slv(TerLAngMin, lay->angMin/90.f);  Slv(TerLHMin, (lay->hMin+300.f)/600.f);
		Slv(TerLAngMax, lay->angMax/90.f);	Slv(TerLHMax, (lay->hMax+300.f)/600.f);
		Slv(TerLAngSm, lay->angSm/90.f);	Slv(TerLHSm, lay->hSm/200.f);
		Slv(TerLNoise, (lay->noise+2.f)/4.f);
		chkTerLNoiseOnly->setStateSelected(lay->bNoiseOnly);
	}
	//  scale layer
	ScrollBar* sl = (HScrollPtr)mWndOpts->findWidget("TerLScale");
	if (sl)  sl->setVisible(bTerLay);
	if (bTerLay)  {
		if (edTerLScale)  edTerLScale->setCaption(toStr(scale));
		editTerLScale(edTerLScale);  }
	if (valTerLAll)
		valTerLAll->setCaption("Used: "+toStr(sc.td.layers.size()));
	
	//  Terrain Particles
	edLDust->setCaption(toStr(lay->dust));	edLDustS->setCaption(toStr(lay->dustS));
	edLMud->setCaption(toStr(lay->mud));	edLSmoke->setCaption(toStr(lay->smoke));
	edLTrlClr->setCaption(toStr(lay->tclr));
	
	//  Surfaces
	int i = idTerLay;  //bTerLay ? idTerLay+1 : 0;  // road at 0
	cmbSurfType->setIndexSelected(su[i].type);
	edSuBumpWave->setCaption(toStr(su[i].bumpWaveLength));
	edSuBumpAmp->setCaption(toStr(su[i].bumpAmplitude));
	edSuRollDrag->setCaption(toStr(su[i].rollingDrag));
	edSuFrict->setCaption(toStr(su[i].frictionNonTread));
	edSuFrict2->setCaption(toStr(su[i].frictionTread));  //-not used, rollResistanceCoefficient too
	noBlendUpd = false;
}

void App::editTerTriSize(EditPtr ed)
{
	Real r = std::max(0.1f, s2r(ed->getCaption()) );
	sc.td.fTriangleSize = r;  sc.td.UpdVals();

	ScrollBar* sl = (HScrollPtr)mWndOpts->findWidget("TerTriSize");  // set slider
	size_t v = std::min(1.f, powf((r -0.1f)/5.9f, 0.5f) )*res;
	if (sl)  sl->setScrollPosition(v);
	// result val text
	const char* str = tabsHmap->getItemSelected()->getCaption().asUTF8_c_str();  int size = atoi(str);
	if (valTerTriSize){  Fmt(s, "%4.2f", sc.td.fTriangleSize * size);	valTerTriSize->setCaption(s);  }
}
// |
void App::slTerTriSize(SL)
{
	Real v = 0.1f + 5.9f * powf(val/res, 2.f);
	sc.td.fTriangleSize = v;  sc.td.UpdVals();
	if (edTerTriSize)  edTerTriSize->setCaption(toStr(v));  // set edit
	// result val text
	const char* str = tabsHmap->getItemSelected()->getCaption().asUTF8_c_str();  int size = atoi(str);
	if (valTerTriSize){  Fmt(s, "%4.2f", sc.td.fTriangleSize * size);	valTerTriSize->setCaption(s);  }
}

void App::tabHmap(TabPtr wp, size_t id)
{
	const char* str = tabsHmap->getItemSelected()->getCaption().asUTF8_c_str();  int size = atoi(str);
	if (valTerTriSize){  Fmt(s, "%4.2f", sc.td.fTriangleSize * size);	valTerTriSize->setCaption(s);  }
}

void App::btnTerrainNew(WP)
{
	const char* str = tabsHmap->getItemSelected()->getCaption().asUTF8_c_str();  int size = atoi(str);
	if (valTerTriSize){  Fmt(s, "%4.2f", sc.td.fTriangleSize * size);	valTerTriSize->setCaption(s);  }

	sc.td.iVertsX = size+1;  sc.td.UpdVals();  // new hf

	float* hfData = new float[sc.td.iVertsX * sc.td.iVertsY];
	int siz = sc.td.iVertsX * sc.td.iVertsY * sizeof(float);
	String name = TrkDir() + "heightmap-new.f32";

	//  generate Hmap
	for (int j=0; j < sc.td.iVertsY; ++j)
	{
		int a = j * sc.td.iVertsX;
		for (int i=0; i < sc.td.iVertsX; ++i,++a)
			hfData[a] = 0.f;  //sc.td.getHeight(i,j);
	}
	std::ofstream of;
	of.open(name.c_str(), std::ios_base::binary);
	of.write((const char*)&hfData[0], siz);
	of.close();

	delete[] hfData;
	bNewHmap = true;	UpdateTrack();
}

//  Terrain  generate  --------------------------------
void App::btnTerGenerate(WP)
{
	float* hfData = new float[sc.td.iVertsX * sc.td.iVertsY];
	int siz = sc.td.iVertsX * sc.td.iVertsY * sizeof(float);
	String name = TrkDir() + "heightmap-new.f32";
	float s = sc.td.fTriangleSize*0.001f,
		ox = pSet->gen_ofsx *s*sc.td.iVertsX, oy = pSet->gen_ofsy *s*sc.td.iVertsY;

	//  generate noise terrain hmap
	for (int j=0; j < sc.td.iVertsY; ++j)
	{	int a = j * sc.td.iVertsX;
		for (int i=0; i < sc.td.iVertsX; ++i,++a)
		{
			float y = Noise(i*s-oy, j*s+ox, pSet->gen_freq, pSet->gen_oct, pSet->gen_persist);
			y = y >= 0.f ? powf(y, pSet->gen_pow) : -powf(-y, pSet->gen_pow);
			hfData[a] = y * pSet->gen_scale;
		}
	}

	std::ofstream of;
	of.open(name.c_str(), std::ios_base::binary);
	of.write((const char*)&hfData[0], siz);
	of.close();

	delete[] hfData;
	bNewHmap = true;	UpdateTrack();
}

void App::slTerGenScale(SL)
{
	float v = 60.f * powf(val/res, 2.f);	if (bGI)  pSet->gen_scale = v;
	if (valTerGenScale){	Fmt(s, "%4.2f", v);  valTerGenScale->setCaption(s);  }
}
void App::slTerGenOfsX(SL)
{
	float v = -2.f + 4.f * val/res;		if (bGI)  pSet->gen_ofsx = v;
	if (valTerGenOfsX){	Fmt(s, "%5.3f", v);  valTerGenOfsX->setCaption(s);  }
}
void App::slTerGenOfsY(SL)
{
	float v = -2.f + 4.f * val/res;		if (bGI)  pSet->gen_ofsy = v;
	if (valTerGenOfsY){	Fmt(s, "%5.3f", v);  valTerGenOfsY->setCaption(s);  }
}

void App::slTerGenFreq(SL)
{
	float v = 0.7f * val/res;	if (bGI)  pSet->gen_freq = v;
	if (valTerGenFreq){	Fmt(s, "%5.3f", v);  valTerGenFreq->setCaption(s);  }
}
void App::slTerGenOct(SL)
{
	if (bGI)  pSet->gen_oct = val;
	if (valTerGenOct){	Fmt(s, "%lu", val);  valTerGenOct->setCaption(s);  }
}
void App::slTerGenPers(SL)
{
	float v = 0.7f * val/res;	if (bGI)  pSet->gen_persist = v;
	if (valTerGenPers){	Fmt(s, "%5.3f", v);  valTerGenPers->setCaption(s);  }
}
void App::slTerGenPow(SL)
{
	float v = 6.f * powf(val/res, 2.f);		if (bGI)  pSet->gen_pow = v;
	if (valTerGenPow){	Fmt(s, "%5.3f", v);  valTerGenPow->setCaption(s);  }
}



///  Terrain layers  -----------------------------
//
void App::chkTerLayOn(WP wp)
{
	if (!bTerLay)  return;
	sc.td.layersAll[idTerLay].on = !sc.td.layersAll[idTerLay].on;
	ButtonPtr chk = wp->castType<Button>();
	chk->setStateSelected(sc.td.layersAll[idTerLay].on);
	sc.td.UpdLayers();
	if (valTerLAll)
		valTerLAll->setCaption("Used: "+toStr(sc.td.layers.size()));
	//  force update, blendmap sliders crash if not
	UpdateTrack();
}

void App::chkTexNormAutoOn(WP wp)
{
	bTexNormAuto = !bTexNormAuto;
	ButtonPtr chk = wp->castType<Button>();
	chk->setStateSelected(bTexNormAuto);
}

void App::comboTexDiff(ComboBoxPtr cmb, size_t val)
{
	String s = cmb->getItemNameAt(val);
	if (bTerLay)  sc.td.layersAll[idTerLay].texFile = s;

	String sTex,sNorm, sExt;
	StringUtil::splitBaseFilename(s,sTex,sExt);
	sNorm = sTex + "_nh." + sExt;  //same ext

	//  auto norm
	if (bTexNormAuto)
	{	cmbTexNorm->setIndexSelected( cmbTexNorm->findItemIndexWith(sNorm) );
		if (bTerLay)  sc.td.layersAll[idTerLay].texNorm = sNorm;  }
	    
	//  tex image
    imgTexDiff->setImageTexture(sTex + "_prv.png");
}

void App::comboTexNorm(ComboBoxPtr cmb, size_t val)
{
	String s = cmb->getItemNameAt(val);
	if (bTerLay)  sc.td.layersAll[idTerLay].texNorm = s;
}

void App::editTerLScale(EditPtr ed)
{
	Real r = std::max(0.01f, s2r(ed->getCaption()) );
	if (bTerLay)  sc.td.layersAll[idTerLay].tiling = r;

	ScrollBar* sl = (HScrollPtr)mWndOpts->findWidget("TerLScale");  // set slider
	size_t v = std::min(1.f,std::max(0.f, powf((r - 2.0f)/9.0f, 1.f/1.5f) ))*res;
	if (sl)  sl->setScrollPosition(v);
}
// |
void App::slTerLScale(SL)  //  scale layer
{
	Real v = 2.0f + 9.0f * powf(val/res, 1.5f);  // 0.1 + 89.9, 1 + 19
	if (bTerLay && bGI)  sc.td.layersAll[idTerLay].tiling = v;
	if (edTerLScale)  edTerLScale->setCaption(toStr(v));  // set edit
}


///  Terrain BlendMap  -----------------------------
//
void App::slTerLAngMin(SL)
{
	float v = 90.f * val/res;
	if (bTerLay && bGI)  sc.td.layersAll[idTerLay].angMin = v;
	if (valTerLAngMin){	Fmt(s, "%4.0f", v);  valTerLAngMin->setCaption(s);  }
	if (terrain && bGI && !noBlendUpd)  bTerUpdBlend = true;  //initBlendMaps(terrain);
}
void App::slTerLAngMax(SL)
{
	float v = 90.f * val/res;
	if (bTerLay && bGI)  sc.td.layersAll[idTerLay].angMax = v;
	if (valTerLAngMax){	Fmt(s, "%4.0f", v);  valTerLAngMax->setCaption(s);  }
	if (terrain && bGI && !noBlendUpd)  bTerUpdBlend = true;
}
void App::slTerLAngSm(SL)
{
	float v = 90.f * val/res;
	if (bTerLay && bGI)  sc.td.layersAll[idTerLay].angSm = v;
	if (valTerLAngSm){	Fmt(s, "%4.0f", v);  valTerLAngSm->setCaption(s);  }
	if (terrain && bGI && !noBlendUpd)  bTerUpdBlend = true;
}

void App::slTerLHMin(SL)
{
	float v = -300.f + 600.f * val/res;
	if (bTerLay && bGI)  sc.td.layersAll[idTerLay].hMin = v;
	if (valTerLHMin){	Fmt(s, "%4.0f", v);  valTerLHMin->setCaption(s);  }
	if (terrain && bGI && !noBlendUpd)  bTerUpdBlend = true;
}
void App::slTerLHMax(SL)
{
	float v = -300.f + 600.f * val/res;
	if (bTerLay && bGI)  sc.td.layersAll[idTerLay].hMax = v;
	if (valTerLHMax){	Fmt(s, "%4.0f", v);  valTerLHMax->setCaption(s);  }
	if (terrain && bGI && !noBlendUpd)  bTerUpdBlend = true;
}
void App::slTerLHSm(SL)
{
	float v = 200.f * val/res;
	if (bTerLay && bGI)  sc.td.layersAll[idTerLay].hSm = v;
	if (valTerLHSm){	Fmt(s, "%4.0f", v);  valTerLHSm->setCaption(s);  }
	if (terrain && bGI && !noBlendUpd)  bTerUpdBlend = true;
}

void App::slTerLNoise(SL)
{
	float v = -2.f + 4.f * val/res;
	if (bTerLay && bGI)  sc.td.layersAll[idTerLay].noise = v;
	if (valTerLNoise){	Fmt(s, "%4.2f", v);  valTerLNoise->setCaption(s);  }
	if (terrain && bGI && !noBlendUpd)  bTerUpdBlend = true;
}

void App::chkTerLNoiseOnlyOn(WP wp)
{
	if (!bTerLay)  return;
	sc.td.layersAll[idTerLay].bNoiseOnly = !sc.td.layersAll[idTerLay].bNoiseOnly;
	ButtonPtr chk = wp->castType<Button>();
	chk->setStateSelected(sc.td.layersAll[idTerLay].bNoiseOnly);
	if (terrain && bGI && !noBlendUpd)  bTerUpdBlend = true;
}


///  Terrain Particles  -----------------------------
//
void App::editLDust(EditPtr ed)
{
	Real r = s2r(ed->getCaption());
	TerLayer* l = !bTerLay ? &sc.td.layerRoad : &sc.td.layersAll[idTerLay];
	String n = ed->getName();

		 if (n=="LDust")   l->dust = r;		else if (n=="LDustS")  l->dustS = r;
	else if (n=="LMud")    l->mud = r;		else if (n=="LSmoke")  l->smoke = r;
}
void App::editLTrlClr(EditPtr ed)
{
	ColourValue c = s2c(ed->getCaption());
	if (!bTerLay)   sc.td.layerRoad.tclr = c;
	else  sc.td.layersAll[idTerLay].tclr = c;
}

void App::comboParDust(ComboBoxPtr cmb, size_t val)
{
	String s = cmb->getItemNameAt(val);
	String n = cmb->getName();
		 
		 if (n=="CmbParDust")   sc.sParDust = s;
	else if (n=="CmbParMud")    sc.sParMud = s;
	else if (n=="CmbParSmoke")  sc.sParSmoke = s;
}


///  Terrain Surface  -----------------------------
//
void App::comboSurfType(ComboBoxPtr cmb, size_t val)
{
	int i = idTerLay;  //bTerLay ? idTerLay+1 : 0;  // road at 0
	su[i].setType(i);
}

void App::editSurf(EditPtr ed)
{
	Real r = s2r(ed->getCaption());
	int i = idTerLay;  //bTerLay ? idTerLay+1 : 0;

		if (ed == edSuBumpWave)   su[i].bumpWaveLength = r;
	else if (ed == edSuBumpAmp)   su[i].bumpAmplitude = r;
	else if (ed == edSuRollDrag)  su[i].rollingDrag = r;
	else if (ed == edSuFrict)     su[i].frictionNonTread = r;
	else if (ed == edSuFrict2)    su[i].frictionTread = r;
}


//  [Vegetation]
//-----------------------------------------------------------------------------------------------------------

void App::editTrGr(EditPtr ed)
{
	Real r = s2r(ed->getCaption());
	String n = ed->getName();

	if (n=="GrassDens")  sc.densGrass = r;	else if (n=="TreesDens")  sc.densTrees = r;
	else if (n=="GrPage")  sc.grPage = r;	else if (n=="GrDist")  sc.grDist = r;
	else if (n=="TrPage")  sc.trPage = r;	else if (n=="TrDist")  sc.trDist = r;

	else if (n=="GrMinX")  sc.grMinSx = r;	else if (n=="GrMaxX")  sc.grMaxSx = r;
	else if (n=="GrMinY")  sc.grMinSy = r;	else if (n=="GrMaxY")  sc.grMaxSy = r;

	else if (n=="GrSwayDistr")  sc.grSwayDistr = r;
	else if (n=="GrSwayLen")  sc.grSwayLen = r;	else if (n=="GrSwaySpd")  sc.grSwaySpeed = r;
	else if (n=="TrRdDist")  sc.trRdDist = r;	else if (n=="TrImpDist")  sc.trDistImp = r;
	else if (n=="GrDensSmooth")  sc.grDensSmooth = r;
	else if (n=="GrTerMaxAngle")  sc.grTerMaxAngle = r;
}

///  Vegetation layers  -----------------------------

void App::tabPgLayers(TabPtr wp, size_t id)
{
	idPgLay = id;  // help var
	const PagedLayer& lay = sc.pgLayersAll[id];

	chkPgLay->setStateSelected(lay.on);
	cmbPgLay->setIndexSelected( cmbPgLay->findItemIndexWith(lay.name) );
	if (imgPaged)	imgPaged->setImageTexture(lay.name + ".png");
	if (valLTrAll)
		valLTrAll->setCaption("Used: "+toStr(sc.pgLayers.size()));

	//  set slider values
	ScrollBar* sl;  size_t v;
	Slv(LTrDens, powf((lay.dens-0.001f) /1.0f, 0.5f));
	Slv(LTrRdDist, Real(lay.addTrRdDist) /res);

	Slv(LTrMinSc, powf(lay.minScale /6.0f, 1.f/3.f));
	Slv(LTrMaxSc, powf(lay.maxScale /6.0f, 1.f/3.f));
	Slv(LTrWindFx, powf(lay.windFx /12.0f, 1.f/3.f));
	Slv(LTrWindFy, powf(lay.windFy /12.0f, 1.f/3.f));
	Slv(LTrMaxTerAng, powf(lay.maxTerAng /90.0f, 1.f/2.f));
	if (edLTrMinTerH)  edLTrMinTerH->setCaption(toStr(lay.minTerH));
}

void App::chkPgLayOn(WP wp)
{
	sc.pgLayersAll[idPgLay].on = !sc.pgLayersAll[idPgLay].on;
	sc.UpdPgLayers();
	ButtonPtr chk = wp->castType<Button>();
	chk->setStateSelected(sc.pgLayersAll[idPgLay].on);
	if (valLTrAll)
		valLTrAll->setCaption("Used: "+toStr(sc.pgLayers.size()));
}

void App::comboPgLay(ComboBoxPtr cmb, size_t val)
{
	String s = cmb->getItemNameAt(val);
	sc.pgLayersAll[idPgLay].name = s;
	if (imgPaged)	imgPaged->setImageTexture(s + ".png");
}

void App::slLTrDens(SL)  //  sliders
{
	Real v = 0.001f + 1.0f * powf(val/res, 2.f);  sc.pgLayersAll[idPgLay].dens = v;
	if (valLTrDens){  Fmt(s, "%5.3f", v);  valLTrDens->setCaption(s);  }
}
void App::slLTrRdDist(SL)
{
	sc.pgLayersAll[idPgLay].addTrRdDist = val;
	if (valLTrRdDist)  valLTrRdDist->setCaption(toStr(val));
}

void App::slLTrMinSc(SL)
{
	Real v = 6.0f * powf(val/res, 3.f);		sc.pgLayersAll[idPgLay].minScale = v;
	if (valLTrMinSc){  Fmt(s, "%5.3f", v);  valLTrMinSc->setCaption(s);  }
}
void App::slLTrMaxSc(SL)
{
	Real v = 6.0f * powf(val/res, 3.f);		sc.pgLayersAll[idPgLay].maxScale = v;
	if (valLTrMaxSc){  Fmt(s, "%5.3f", v);  valLTrMaxSc->setCaption(s);  }
}

void App::slLTrWindFx(SL)
{
	Real v = 12.0f * powf(val/res, 3.f);	sc.pgLayersAll[idPgLay].windFx = v;
	if (valLTrWindFx){  Fmt(s, "%5.3f", v);  valLTrWindFx->setCaption(s);  }
}
void App::slLTrWindFy(SL)
{
	Real v = 12.0f * powf(val/res, 3.f);	sc.pgLayersAll[idPgLay].windFy = v;
	if (valLTrWindFy){  Fmt(s, "%5.3f", v);  valLTrWindFy->setCaption(s);  }
}

void App::slLTrMaxTerAng(SL)
{
	Real v = 90.0f * powf(val/res, 2.f);	sc.pgLayersAll[idPgLay].maxTerAng = v;
	if (valLTrMaxTerAng){  Fmt(s, "%5.1f", v);  valLTrMaxTerAng->setCaption(s);  }
}
void App::editLTrMinTerH(EditPtr ed)
{
	sc.pgLayersAll[idPgLay].minTerH = s2r(ed->getCaption());
}



//  [Road]
//-----------------------------------------------------------------------------------------------------------

void App::editTrkDesc(EditPtr ed)
{
	road->sTxtDesc = ed->getCaption();
}

void App::comboRoadMtr(ComboBoxPtr cmb, size_t val)
{
	String sn = cmb->getName().substr(String("RdMtr").length(), cmb->getName().length());
	int id = atoi(sn.c_str())-1;  if (id < 0 || id >= MTRs)  return;

	String s = cmb->getItemNameAt(val);
	road->sMtrRoad[id] = s;  road->RebuildRoad(true);  UpdPSSMMaterials();
}

void App::comboPipeMtr(ComboBoxPtr cmb, size_t val)
{
	String sn = cmb->getName().substr(String("RdMtrP").length(), cmb->getName().length());
	int id = atoi(sn.c_str())-1;  if (id < 0 || id >= MTRs)  return;

	String s = cmb->getItemNameAt(val);
	road->sMtrPipe[id] = s;  road->RebuildRoad(true);  UpdPSSMMaterials();
}

void App::editRoad(EditPtr ed)
{
	if (!road)  return;
	Real r = s2r(ed->getCaption());
	String n = ed->getName();

		 if (n=="RdTcMul")		road->tcMul = r;	else if (n=="RdColN")	road->colN = std::max(3.f, r);
	else if (n=="RdLenDim")		road->lenDiv0 = r;	else if (n=="RdColR")	road->colR = r;
	else if (n=="RdWidthSteps")	road->iw0 = r;		else if (n=="RdPwsM")	road->iwPmul = r;
	else if (n=="RdHeightOfs")	road->fHeight = r;	else if (n=="RdPlsM")	road->ilPmul = r;
	else if (n=="RdSkirtLen")	road->skLen = r;	else if (n=="RdSkirtH")	road->skH = r;
	else if (n=="RdMergeLen")	road->setMrgLen = r;
	else if (n=="RdLodPLen")	road->lposLen = r;
	//road->RebuildRoad(true);  //on Enter-
}


//  [Settings]  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

//  Startup
void App::chkOgreDialog(WP wp){		ChkEv(ogre_dialog);	}
void App::chkAutoStart(WP wp){		ChkEv(autostart);	}
void App::chkEscQuits(WP wp){		ChkEv(escquit);		}

void App::slMiniUpd(SL)
{
	pSet->mini_skip = val;
	if (valMiniUpd){	Fmt(s, "%lu", val);	valMiniUpd->setCaption(s);  }
}

void App::slSizeRoadP(SL)
{
	Real v = 0.1f + 11.9f * val/res;  pSet->road_sphr = v;
	if (valSizeRoadP){	Fmt(s, "%4.2f", v);	valSizeRoadP->setCaption(s);  }
	if (road)
	{	road->fMarkerScale = v;
		road->UpdAllMarkers();  }
}

void App::slCamInert(SL)
{
	Real v = val/res;  pSet->cam_inert = v;
	if (valCamInert){	Fmt(s, "%4.2f", v);	valCamInert->setCaption(s);  }
}
void App::slCamSpeed(SL)
{
	Real v = 0.1f + 3.9f * powf(val/res, 1.f);  pSet->cam_speed = v;
	if (valCamSpeed){	Fmt(s, "%4.2f", v);	valCamSpeed->setCaption(s);  }
}

void App::slTerUpd(SL)
{
	pSet->ter_skip = val;
	if (valTerUpd){	Fmt(s, "%lu", val);	valTerUpd->setCaption(s);  }
}

void App::slSizeMinmap(SL)
{
	float v = 0.15f + 1.85f * val/res;	pSet->size_minimap = v;
	if (valSizeMinmap){	Fmt(s, "%4.3f", v);	valSizeMinmap->setCaption(s);  }
	Real sz = pSet->size_minimap;  //int all = 0;
	xm1 = 1-sz/asp, ym1 = -1+sz, xm2 = 1.0, ym2 = -1.0;
	for (int i=0; i < RTs+1; ++i)  if (i != RTs)  {
		if (rt[i].rcMini)  rt[i].rcMini->setCorners(xm1, ym1, xm2, ym2);  }
}

void App::chkMinimap(WP wp)
{	ChkEv(trackmap);
	UpdMiniVis();
	if (ndPos)  ndPos->setVisible(pSet->trackmap);
}

//brush_prv


//  set camera in settings at exit
void App::SaveCam()
{
	if (mCamera) {
		Vector3 p = mCamera->getPosition(), d = mCamera->getDirection();
		pSet->cam_x = p.x;   pSet->cam_y = p.y;   pSet->cam_z = p.z;
		pSet->cam_dx = d.x;  pSet->cam_dy = d.y;  pSet->cam_dz = d.z;
	}
}

//  set predefined camera view
void App::btnSetCam(WP wp)
{
	String s = wp->getName();
	Real y0 = 20, xz = sc.td.fTerWorldSize*0.5f, r = 45.f * 0.5f*PI_d/180.f, yt = xz / Math::Tan(r);

		 if (s=="CamView1")	{	mCameraT->setPosition(xz*0.8,60,0);  mCameraT->setDirection(-1,-0.3,0);  }
	else if (s=="CamView2")	{	mCameraT->setPosition(xz*0.6,80,xz*0.6);  mCameraT->setDirection(-1,-0.5,-1);  }
	else if (s=="CamView3")	{	mCameraT->setPosition(-xz*0.7,80,-xz*0.5);  mCameraT->setDirection(0.8,-0.5,0.5);  }
	else if (s=="CamView4")	{
		Vector3 cp = ndCar->getPosition();  float cy = ndCar->getOrientation().getYaw().valueRadians();
		Vector3 cd = Vector3(cosf(cy),0,-sinf(cy));
		mCameraT->setPosition(cp - cd * 15 + Vector3(0,7,0));  cd.y = -0.3f;
		mCameraT->setDirection(cd);  }

	else if (s=="CamTop")	{	mCameraT->setPosition(0,yt,0);  mCameraT->setDirection(-0.0001,-1,0);  }
	else if (s=="CamLeft")	{	mCameraT->setPosition(0,y0, xz);  mCameraT->setDirection(0,0,-1);  }
	else if (s=="CamRight")	{	mCameraT->setPosition(0,y0,-xz);  mCameraT->setDirection(0,0, 1);  }
	else if (s=="CamFront")	{	mCameraT->setPosition( xz,y0,0);  mCameraT->setDirection(-1,0,0);  }
	else if (s=="CamBack")	{	mCameraT->setPosition(-xz,y0,0);  mCameraT->setDirection( 1,0,0);  }
}
