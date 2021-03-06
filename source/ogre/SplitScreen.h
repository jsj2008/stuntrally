#ifndef _SplitScreenManager_h_
#define _SplitScreenManager_h_

#include <OgreRenderTargetListener.h>
#include <list>

class SETTINGS;
namespace Ogre {  class SceneNode;  class Root;  class SceneManager;  class RenderWindow;  class Viewport;  }

/*
 * SplitScreenManager can be used to split up the screen into 2 - 4 areas.
 * This is useful for "Hotseat" game mode (Multiple players driving against each other on the same PC)
 * 
 * For every player a viewport and a camera will be created.
 * Also there is one transparent fullscreen viewport on top of the others, for the GUI.
 * 
 * One instance of SplitScreenManager is created when the game is started,
 * and it will be destroyed when game quits.
 */

class SplitScreenManager : public Ogre::RenderTargetListener
{
public:
	// Constructor, only assign members
	SplitScreenManager(Ogre::SceneManager* sceneMgr, Ogre::RenderWindow* window, SETTINGS* set);
	~SplitScreenManager();
	
	// Number of viewports / cameras
	int mNumViewports;
	
	// Lists for player viewports & cameras
	std::list<Ogre::Viewport*> mViewports;
	std::list<Ogre::Camera*> mCameras;
	
	// Gui viewport & scene manager
	Ogre::SceneManager* mGuiSceneMgr;
	Ogre::Viewport* mGuiViewport;
	
	// Hud viewports & scene manager & cameras
	std::list<Ogre::Viewport*> mHUDViewports;
	Ogre::Camera* mHUDCamera;
	Ogre::SceneManager* mHUDSceneMgr;
	
	// This method should always be called after mNumPlayers is changed.
	// It will create new viewports and cameras and arrange them.
	void Align();
	
	// Destroy viewports & cameras
	void CleanUp();
	
	// Adjust viewport size / camera aspect ratio
	// Should be called whenever the window size changes
	void AdjustRatio();
	
	// Set background color for all viewports
	void SetBackground(const Ogre::ColourValue& color);
	
	// Update the view distance for all cameras.
	// This will be called when the view distance slider in gui is changed.
	void UpdateCamDist();
	
	void preViewportUpdate(const Ogre::RenderTargetViewportEvent& evt);
	void postViewportUpdate(const Ogre::RenderTargetViewportEvent& evt);

	class App* pApp;
private:
	SETTINGS* pSet;

	// Scene manager to use
	Ogre::SceneManager* mSceneMgr;
	
	// Render window to use
	Ogre::RenderWindow* mWindow;
};

#endif
