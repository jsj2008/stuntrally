#include "pch.h"
#include "Defines.h"
#include "../vdrift/game.h"
#include "OgreGame.h"
#include <OIS/OIS.h>
#include "../oisb/OISB.h"

#include <OgreRoot.h>
using namespace MyGUI;
using namespace Ogre;


///  Gui Init - input tab
//----------------------------------------------------------------------------------------------------------------------------------
String App::GetInputName(const String& sName)
{
	Ogre::vector<String>::type vs = StringUtil::split(sName, "/");
	if (vs.size() != 2)
		return sName;

	if (vs[0] == "Keyboard")
		return StrFromKey(sName);
	else
		return vs[1];
}

void App::InitInputGui()
{
	//  Log all devices to log.txt
	OISB::System& sys = OISB::System::getSingleton();
	pGame->info_output << "--------------------------------------  Input devices  BEGIN" << std::endl;
	//sys.dumpActionSchemas(pGame->info_output);
	sys.dumpDevices(pGame->info_output);
	pGame->info_output << "--------------------------------------  Input devices  END" << std::endl;

	TabItemPtr inpTabAll = mGUI->findWidget<TabItem>("InputTabAll");  if (!inpTabAll)  return;
	TabPtr inputTab = (TabPtr)inpTabAll->findWidget("InputTab");  if (!inputTab)  return;


	///  joystick selection combo (for bind name, when more joys)
	ComboBoxPtr cmbJoy = (ComboBoxPtr)inpTabAll->findWidget("joystickSel");
	if (cmbJoy)
	{
		//joysticks->addItem(TR("#{InputNoJS}"));//-
		//joysticks->setIndexSelected(0);
		int jnum = sys.mJoysticks.size();
		for (int i=0; i < jnum; ++i)
			cmbJoy->addItem( sys.mJoysticks[i]->getName() );

		cmbJoy->setEditReadOnly(true);
		cmbJoy->eventComboChangePosition = newDelegate(this, &App::cmbJoystick);
		if (jnum > 0)  {  cmbJoy->setIndexSelected(0);  cmbJoystick(cmbJoy, 0);  }
	}

	//  labels that print the last pressed joystick button / last moved axis
	txtJAxis = (StaticTextPtr)inpTabAll->findWidget("axisOutput");
	txtJBtn = (StaticTextPtr)inpTabAll->findWidget("buttonOutput");


	///  insert a tab item for every schema (4players,global)
	std::map<OISB::String, OISB::ActionSchema*> schemas = sys.mActionSchemas;
	for (std::map<OISB::String, OISB::ActionSchema*>::const_iterator it = schemas.begin(); it != schemas.end(); it++)
	{
		const OISB::String& sPlr = (*it).first;
		TabItemPtr tabitem = inputTab->addItem( TR("#{InputMap" + sPlr + "}") );

		#define CreateText(x,y, w,h, name, text)  {  StaticTextPtr txt =  \
			tabitem->createWidget<StaticText>("StaticText", x,y, w,h, Align::Relative, name);  \
			if (txt)  txt->setCaption(text);  }
		
		///  Headers (Key 1, Key 2)
		CreateText(200,10, 200,24, "staticText_" + sPlr, TR("#88AAFF#{InputKey1}"));
		//CreateText(340,10, 200,24, "staticText_" + sPlr, TR("#88AAFF#{InputKey2}"));
		
		///  ------ custom action sorting ----------------
		int i = 0, y = 0, ya = 26 / 2, yc1=0,yc2=0,yc3=0;
		std::map <std::string, int> yRow;
		// player
		yRow["Throttle"] = y;	y+=2;	yRow["Brake"] = y;		y+=2;
		yRow["Steering"] = y;	y+=2+1 +2;
		yRow["HandBrake"] = y;	y+=2;	yRow["Boost"] = y;		y+=2;
		yRow["Flip"] = y;
		yRow["FlipRight"] = y;	y+=2;	yRow["FlipLeft"] = y;	y+=2 +1;
		yRow["ShiftUp"] = y;	y+=2;	yRow["ShiftDown"] = y;	y+=2;
		// general
		y = 0;
		yRow["ShowOptions"] = y; y+=2+1;
		yRow["PrevTab"] = y;     y+=2;	yRow["NextTab"] = y;    y+=2+1;
		yRow["RestartGame"] = y; y+=2+1;  yc1 = 40 + ya * y;
		yRow["PrevCamera"] = y;  y+=2;    yc2 = 40 + ya * y;
		yRow["NextCamera"] = y;  y+=2+1;  yc3 = 40 + ya * y;

		bool playerTab = Ogre::StringUtil::startsWith( sPlr, "player");
		if (!playerTab)
		{	//  camera infos
			CreateText(460, yc1, 280, 24, "txtcam1", TR("#C0D8F0#{InputCameraTxt1}"));
			CreateText(460, yc2, 280, 24, "txtcam1", TR("#C0D8F0#{InputCameraTxt2}"));
			//  replay controls info text
			CreateText(20, yc3+1*ya, 500, 24, "txtrpl1", TR("#A0D8FF#{InputRplCtrl0}"));
			CreateText(40, yc3+3*ya, 500, 24, "txtrpl2", TR("#90C0FF#{InputRplCtrl1}"));
			CreateText(40, yc3+5*ya, 500, 24, "txtrpl3", TR("#90C0FF#{InputRplCtrl2}"));
			CreateText(40, yc3+7*ya, 500, 24, "txtrpl4", TR("#90C0FF#{InputRplCtrl3}"));
		}
		
		///  Actions  ------------------------------------------------
		for (std::map<OISB::String, OISB::Action*>::const_iterator
			ait = (*it).second->mActions.begin();
			ait != (*it).second->mActions.end(); ++ait,++i)
		{
			const OISB::String& sAct = (*ait).first;
			OISB::Action* act = (*ait).second;

			//  button size and columns positon
			const int sx = 130, sy = 24, x0 = 20, x1 = 160, x2 = 300;
			const String& name = (*ait).second->getName();
			y = 40 + ya * yRow[name];

			//  description label
			StaticTextPtr desc = tabitem->createWidget<StaticText>("StaticText",
				x0, y+5, sx+70, sy,  Align::Relative,
				"staticText_" + sAct );
			desc->setCaption( TR("#{InputMap" + name + "}") );
		
			///  Keyboard binds  --------------------------------
			//  get information about binds from OISB and set variables how the rebind buttons should be created
			std::string skey1 = TR("#{InputKeyUnassigned}");
			std::string skey2 = TR("#{InputKeyUnassigned}");
			
			//  bound key(s)
			bool button2 = act->getName() == "Steering" || act->getName() == "Flip";  // full
			if (act->mBindings.size() > 0 && act->mBindings.front()->getNumBindables() > 0 && act->mBindings.front()->getBindable(0) && act->mBindings.front()->getBindable(0) != (OISB::Bindable*)1)
			if (act->getActionType() == OISB::AT_TRIGGER)
			{
				skey1 = GetInputName(act->mBindings.front()->getBindable(0)->getBindableName());
			}
			else if (act->getActionType() == OISB::AT_ANALOG_AXIS)
			{
				//  look for increase/decrease binds
				OISB::Bindable* increase = NULL, *decrease = NULL, *none = NULL;
				for (std::vector<std::pair<String, OISB::Bindable*> >::const_iterator
					bnit = act->mBindings.front()->mBindables.begin();
					bnit != act->mBindings.front()->mBindables.end(); bnit++)
				{
					if ((*bnit).first == "inc")			increase = (*bnit).second;
					else if ((*bnit).first == "dec")	decrease = (*bnit).second;
					else none = (*bnit).second;
				}
				if (increase)  skey1 = GetInputName(increase->getBindableName());
				if (decrease)  skey2 = GetInputName(decrease->getBindableName());
				if (none)
					(button2 ? skey2 : skey1) = GetInputName(none->getBindableName());
			}
				
			//  create buttons  ----------------
			ButtonPtr btn1 = tabitem->createWidget<Button>("Button", /*button2 ? x2 :*/
				x1, button2 ? (y + ya*2) : y, sx, sy,  Align::Relative,
				"inputbutton_" + sAct + "_" + sPlr + "_1");
			btn1->setCaption( skey1 );
			btn1->eventMouseButtonClick = newDelegate(this, &App::inputBindBtnClicked);
			
			if (button2)
			{	ButtonPtr btn2 = tabitem->createWidget<Button>("Button",
					x1, y, sx, sy,  MyGUI::Align::Relative,
					"inputbutton_" + sAct + "_" + sPlr + "_2");
				btn2->setCaption( skey2 );
				btn2->eventMouseButtonClick = MyGUI::newDelegate(this, &App::inputBindBtnClicked);
			}
			
			//  create bars
			StaticImagePtr bar = tabitem->createWidget<StaticImage>("StaticImage",
				x2 + (button2 ? 0 : 64), y+4, button2 ? 128 : 64, 16, MyGUI::Align::Relative,
				"bar_" + sAct + "_" + sPlr);
			bar->setImageTexture("input_bar.png");  bar->setImageCoord(IntCoord(0,0,128,16));
		}
	}
	/**/mWndTabs->setIndexSelected(7);  ///remove
	/**/inputTab->setIndexSelected(1);
	//toggleGui();
}


///  Bind Input
//----------------------------------------------------------------------------------------------------------------------------------

void App::inputBindBtnClicked(Widget* sender)
{
	sender->setCaption( TR("#{InputAssignKey}"));
	// activate key capture mode
	bAssignKey = true;
	pressedKeySender = sender;
	axisCnt = 0;
	MyGUI::PointerManager::getInstance().setVisible(false);
}

void App::InputBind(int key, int button, int axis)
{
	if (!bAssignKey)  return;
	bAssignKey = false;
	MyGUI::PointerManager::getInstance().setVisible(true);

	//  cancel (unbind) on Backspace or Escape
	bool cancel = key == OIS::KC_BACK || key == OIS::KC_ESCAPE;
	
	//  upd key name on button
	bool isKey = key > -1, isAxis = axis > -1;
	String joy = joyName;  // from joy combo
	String skey0 = isKey ? "Keyboard/" + toStr(key) : 
				isAxis ? joy + "/Axis " + toStr(axis) :
						joy + "/Button " + toStr(button);
	pressedKeySender->setCaption(GetInputName(skey0));

	
	//  get action/schema/index from widget name
	Ogre::vector<String>::type ss = StringUtil::split(pressedKeySender->getName(), "_");
	std::string actionName = ss[1], schemaName = ss[2], index = ss[3];
	
	OISB::ActionSchema* schema = OISB::System::getSingleton().mActionSchemas[schemaName];  if (!schema)  return;//
	OISB::Action* action = schema->mActions[actionName];  if (!action)  return;//
	if (action->mBindings.size() == 0)
		action->createBinding();
	OISB::Binding* binding = action->mBindings.front();  if (!binding)  return;//

	
	///  change AnalogAxis params  key/button <-> axis
	if (action->getActionType() == OISB::AT_ANALOG_AXIS)
	{
		bool full = action->getName() == "Steering" || action->getName() == "Flip";  //-1..1
		OISB::AnalogAxisAction* act = (OISB::AnalogAxisAction*)action;
		act->setProperty("AnalogEmulator", isAxis ? "" : "Linear");  act->setUseAbsoluteValues(/*isAxis*/false);
		act->setProperty("MinValue", full ? -1 : 0);  act->setProperty("MaxValue", 1);
		act->setProperty("InverseMul", 1);
		act->setProperty("Sensitivity", 1);		// restore defaults
		OISB::AnalogEmulator* emu = act->getAnalogEmulator();  if (emu)  {
			emu->setProperty("DecSpeed", 5);		emu->setProperty("IncSpeed", 5);
			emu->setProperty("ReturnEnabled", 1);	emu->setProperty("ReturnValue", 0);
			emu->setProperty("ReturnDecSpeed", 5);	emu->setProperty("ReturnIncSpeed", 5);  }
	}
	
	//  save keys
	String decKey = "", incKey = "";
	size_t num = binding->getNumBindables();
	for (int i = 0; i < num; ++i)
	{
		OISB::Bindable* bind = binding->getBindable(i);
		String name = bind->getBindableName();
		String role = binding->getRole(bind);

		if (role == "dec")  decKey = name;
		if (role == "inc")  incKey = name;
	}

	//  clear, unbind
	for (int i = num-1; i >= 0; --i)
		binding->unbind(binding->getBindable(i));

	//  change
	String skey = cancel ? "" : skey0;
		 if (index == "1")  incKey = skey;  // lower btn - inc
	else if (index == "2")  decKey = skey;  // upper btn - dec

	//  update, bind  key/button
	if (!isAxis)
	{	if (incKey != "")	binding->bind(incKey, "inc");
		if (decKey != "")	binding->bind(decKey, "dec");
				
		//  update button labels  . . . . . . . 
		MyGUI::ButtonPtr b1 = mGUI->findWidget<MyGUI::Button>("inputbutton_" + actionName + "_" + schemaName + "_" + "1", "", false);
		MyGUI::ButtonPtr b2 = mGUI->findWidget<MyGUI::Button>("inputbutton_" + actionName + "_" + schemaName + "_" + "2", "", false);
		if (b1)  b1->setCaption(GetInputName(incKey));
		if (b2)  b2->setCaption(GetInputName(decKey));
	}
	else  // axis
	{
		binding->bind(skey,"");
		//todo  detail panel: edits or sld for inversemul, minval, maxval, sensivity-
		//start ok when joy wasnt detected but is in xml, save it too
	}
}


///  update input bars vis,dbg
//-------------------------------------------------------------------------------
void App::UpdateInputBars()
{
	MyGUI::TabPtr inputTab = mGUI->findWidget<Tab>("InputTab");
	if (!inputTab)  return;

	OISB::System& sys = OISB::System::getSingleton();  int sch = 0;
	std::map<OISB::String, OISB::ActionSchema*> schemas = sys.mActionSchemas;
	for (std::map<OISB::String, OISB::ActionSchema*>::const_iterator it = schemas.begin(); it != schemas.end(); ++it,++sch)
	{
		const OISB::String& sPlr = (*it).first;
		if (inputTab->getIndexSelected() != sch)  continue;

		//  Action
		for (std::map<OISB::String, OISB::Action*>::const_iterator
			ait = (*it).second->mActions.begin();
			ait != (*it).second->mActions.end(); ++ait)
		{
			const OISB::String& sAct = (*ait).first;
			OISB::Action* act = (*ait).second;
			float val = -1.f;
			bool full = act->getName() == "Steering" || act->getName() == "Flip";
			//  get val
			if (act->getActionType() == OISB::AT_ANALOG_AXIS)
			{
				OISB::AnalogAxisAction* ac = static_cast<OISB::AnalogAxisAction*>(act);
				if (ac)  val = ac->getAbsoluteValue();
			}else
				val = act->isActive() ? 1.f : 0.f;
				
			std::string sBar = "bar_" + sAct + "_" + sPlr;
			StaticImagePtr bar = (StaticImagePtr)inputTab->findWidget(sBar);
			if (bar)
			{	const int wf = 128, w = 256;  int v = -val * 128, vf = -val * 64, s=512, s0=s/2;
				if (full)	bar->setImageCoord(IntCoord(std::max(0, std::min(s-wf, vf + s0 -wf/2)), 0, wf, 16));
				else		bar->setImageCoord(IntCoord(std::max(0, std::min(s-w, v + s0)), 0, w, 16));
			}
		}
	}
}


///  joysticks events
//
bool App::axisMoved( const OIS::JoyStickEvent &e, int axis )
{
	if (txtJAxis)
	{	int iv = e.state.mAxes[axis].abs;
		float val = iv >= 0 ? iv / 32767.f : iv / 32768.f;
		static char ss[128];
		sprintf(ss, "Moved axis: %d     val: %7.4f", axis, val);
		txtJAxis->setCaption(ss);
	}	
	if (lastAxis != axis)
	{	lastAxis = axis;
		axisCnt = 0;
	}else
	{
		if (axisCnt++ > 10)
		{	axisCnt = 0;
			//  bind when same axis moved few times, omit axes input noise-
			InputBind(-1, -1, axis);
		}
	}
	return true;
}

bool App::buttonPressed( const OIS::JoyStickEvent &e, int button )
{
	if (txtJBtn)  txtJBtn->setCaption("Pressed button: " + toStr(button));

	InputBind(-1, button);

	return true;
}
bool App::buttonReleased( const OIS::JoyStickEvent &e, int button )
{
	return true;
}

void App::cmbJoystick(Widget* sender, size_t val)
{
		ComboBoxPtr cmb = (ComboBoxPtr)sender;
	joyName = cmb->getItemNameAt(val);
}