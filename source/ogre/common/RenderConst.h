#ifndef _Render_Const_h_
#define _Render_Const_h_

#include <OgreRenderQueue.h>


//  Render Queue Groups used  //search for setRenderQueueGroup
//------------------------------------------------------------------------

const Ogre::uint8
	RQG_Sky = Ogre::RENDER_QUEUE_SKIES_EARLY,	// 5
	
	RQG_BatchOpaque  = Ogre::RENDER_QUEUE_MAIN,	// 50  paged geom
	RQG_BatchAlpha   = Ogre::RENDER_QUEUE_6,	// 60  paged geom transparent

	RQG_Fluid        = Ogre::RENDER_QUEUE_6+4,

	RQG_RoadMarkers  = Ogre::RENDER_QUEUE_7,

	RQG_CarGlass     = Ogre::RENDER_QUEUE_7,
	
	RQG_CarTrails    = Ogre::RENDER_QUEUE_8,	//trails after glass
	RQG_CarParticles = Ogre::RENDER_QUEUE_8+2,	//particles after trails
	
	RQG_Weather      = Ogre::RENDER_QUEUE_8+5,
	RQG_PipeGlass    = Ogre::RENDER_QUEUE_8+5,	// glass pipe road`
	
	RQG_CarGhost     = Ogre::RENDER_QUEUE_8+7,

	RQG_Hud1         = Ogre::RENDER_QUEUE_OVERLAY-5,	// 95
	RQG_Hud2         = Ogre::RENDER_QUEUE_OVERLAY-1,
	RQG_Hud3         = Ogre::RENDER_QUEUE_OVERLAY;		// 100


//  Visibility Flags used  //search for setVisibility
//------------------------------------------------------------------------
const Ogre::uint32
	RV_Road = 1,	// road only, for road textures
	RV_Hud = 2,		// hud and markers
	RV_Terrain = 4,	// terrain and fluids, for terrain texture
	RV_Vegetation = 8,  // vegetation, paged geom
	RV_Sky = 32,	// sky, editor only

	RV_Car = 2,			// car,tires in game, (hide in reflection render)
	RV_CarGlass = 16,	// car glass in game, (hide for in car camera)
	RV_MaskReflect = RV_Road + RV_Terrain + RV_Vegetation,  // hide 2: hud, car,glass,tires

	RV_MaskAll = 255,
	RV_MaskPrvCam = 256;

#endif
