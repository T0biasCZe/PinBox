#include "PPUI.h"
#include <cstdio>
#include "ConfigManager.h"
#include "Anim.h"

volatile u32 kDown;
volatile u32 kHeld;
volatile u32 kUp;

volatile u32 last_kDown;
volatile u32 last_kHeld;
volatile u32 last_kUp;

static circlePosition cPos;
static circlePosition cStick;
static touchPosition kTouch;
static touchPosition last_kTouch;
static touchPosition first_kTouchDown;
static touchPosition last_kTouchDown;
volatile u64 holdTime = 0;

static u32 sleepModeState = 0;

//----------------------------------------
// Dialog
//---------------------------------------
static bool mTmpLockTouch = false;
static PopupCallback *mDialogBox;
static PopupCallback *mDialogBoxCallLater;
static DialogBoxOverride mDialogOverride;

static u64 mTmpWaitTimer = 0;
static std::vector<PopupCallback> mPopupList;

static std::string mTemplateInputString = "";

static ServerConfig* mTmpServerConfig = nullptr;

//----------------------------------------
// animations
//----------------------------------------
static u64 mTmpLoadingTimeA = 0;
static u64 mTmpLoadingTimeB = 0;
static u64 mTmpLoadingTimeC = 0;
static int mTmpLoadingDirA = 1;
static int mTmpLoadingDirB = 1;
static int mTmpLoadingDirC = 1;


static const char* UI_INPUT_VALUE[] = { "1", "2", "3", 
										"4", "5", "6", 
										"7", "8", "9", 
										".", "0", ":" };

static const char* UI_KEYBOARD_VALUE[] = { 
	"q", "w", "e", "r", "t", "y", "u", "i", "o", "p",
	"a", "s", "d", "f", "g", "h", "j", "k", "l", "'",
	"z", "x", "c", "v", "b", "n", "m", ",", ".", " ",
};

u32 PPUI::getKeyDown()
{
	return kDown;
}

u32 PPUI::getKeyHold()
{
	return kHeld;
}

u32 PPUI::getKeyUp()
{
	return kUp;
}

circlePosition PPUI::getLeftCircle()
{
	return cPos;
}

circlePosition PPUI::getRightCircle()
{
	return cStick;
}

u32 PPUI::getSleepModeState()
{
	return sleepModeState;
}

void PPUI::UpdateInput()
{
	//----------------------------------------
	// store old input
	last_kDown = kDown;
	last_kHeld = kHeld;
	last_kUp = kUp;
	last_kTouch = kTouch;
	//----------------------------------------
	// scan new input
	kDown = hidKeysDown();
	kHeld = hidKeysHeld();
	kUp = hidKeysUp();
	cPos = circlePosition();
	hidCircleRead(&cPos);
	cStick = circlePosition();
	irrstCstickRead(&cStick);
	kTouch = touchPosition();
	hidTouchRead(&kTouch);

	if(kDown & KEY_TOUCH)
	{
		first_kTouchDown = kTouch;
		last_kTouchDown = touchPosition();
	}
	if(last_kHeld & KEY_TOUCH && kUp & KEY_TOUCH)
	{
		last_kTouchDown = kTouch;
		first_kTouchDown = touchPosition();
		holdTime = 0;
	}
}

bool PPUI::TouchDownOnArea(float x, float y, float w, float h)
{
	if (kDown & KEY_TOUCH || kHeld & KEY_TOUCH)
	{
		if (kTouch.px >= (u16)x && kTouch.px <= (u16)(x + w) && kTouch.py >= (u16)y && kTouch.py <= (u16)(y + h))
		{
			return true;
		}
	}
	return false;
}

bool PPUI::TouchUpOnArea(float x, float y, float w, float h)
{
	if ((last_kDown & KEY_TOUCH || last_kHeld & KEY_TOUCH) && kUp & KEY_TOUCH)
	{
		if (last_kTouch.px >= (u16)x && last_kTouch.px <= (u16)(x + w) && last_kTouch.py >= (u16)y && last_kTouch.py <= (u16)(y + h))
		{
			return true;
		}
	}
	return false;
}

bool PPUI::TouchDown()
{
	return kDown & KEY_TOUCH;
}

bool PPUI::TouchMove()
{
	return kHeld & KEY_TOUCH;
}

bool PPUI::TouchUp()
{
	return last_kHeld & KEY_TOUCH && kUp & KEY_TOUCH;
}

///////////////////////////////////////////////////////////////////////////
// TEXT
///////////////////////////////////////////////////////////////////////////

int PPUI::DrawIdleTopScreen(PPSessionManager* sessionManager)
{
	PPGraphics::Get()->DrawRectangle(0, 0, 400, 240, RGB(26, 188, 156));
	LabelBox(0, 0, 400, 240, "PinBox", RGB(26, 188, 156), RGB(255, 255, 255));
}

int PPUI::DrawBtmServerSelectScreen(PPSessionManager* sessionManager)
{
	PPGraphics::Get()->DrawRectangle(0, 0, 320, 240, RGB(236, 240, 241));
	PPGraphics::Get()->DrawRectangle(0, 0, 320, 35, RGB(26, 188, 156));

	// Screen title
	switch (sessionManager->GetSessionState()) {
	case -1: LabelBox(55, 5, 200, 25, "Status: No Wifi Connection", RGB(26, 188, 156), RGB(255, 255, 255)); break;
	case 0: LabelBox(55, 5, 200, 25, "Status: Ready to Connect", RGB(26, 188, 156), RGB(255, 255, 255)); break;
	case 1: LabelBox(55, 5, 200, 25, "Status: Connecting...", RGB(26, 188, 156), RGB(255, 255, 255)); break;
	case 2: LabelBox(55, 5, 200, 25, "Status: Connected", RGB(26, 188, 156), RGB(255, 255, 255)); break;
	}

	// Quit
	if (FlatColorButton(5, 5, 50, 25, "Quit", RGB(214, 48, 49), RGB(255, 118, 117), RGB(255, 255, 255)))
	{
		OverrideDialogTypeCritical();
		DrawDialogMessage(sessionManager, "Warning", "Are you sure to quit?", [=]()
		{
			// on cancel
			return -1;
		}, [=]()
		{
			// on ok
			return RET_CLOSE_APP;
		});
	}

	// Add new Server
	if (FlatColorButton(260, 5, 50, 25, "Add", RGB(9, 132, 227), RGB(116, 185, 255), RGB(255, 255, 255)))
	{
		AddPopup([=]()
		{
			if(mTmpServerConfig == nullptr)
			{
				mTmpServerConfig = new ServerConfig();
				mTmpServerConfig->name = "My Local PC";
				mTmpServerConfig->ip = "127.0.0.1";
				mTmpServerConfig->port = "1234";
			}
			
			return DrawBtmAddNewServerProfileScreen(sessionManager,
				[=](void* a, void* b)
				{
					delete mTmpServerConfig;
					mTmpServerConfig = nullptr;
					// on cancel
				},
				[=](void* a, void* b)
				{
					// add to list
					ConfigManager::Get()->servers.push_back(ServerConfig(*mTmpServerConfig));
					ConfigManager::Get()->Save(); //TODO: error here when save

					delete mTmpServerConfig;
					mTmpServerConfig = nullptr;
					// on ok
				}
			);
		});
	}

	// List servers
	if(ConfigManager::Get()->servers.size() > 0)
	{
		//TODO: scroll box
		float boxHeight = 40;
		for(int i = 0; i < ConfigManager::Get()->servers.size(); ++i)
		{
			float sx = 40 + boxHeight * i + 5 * i;
			// Draw BG
			PPGraphics::Get()->DrawRectangle(0, sx, 320, boxHeight, RGB(223, 228, 234));

			// Server name
			LabelBoxLeft(70, sx + 2, 150, 20, ConfigManager::Get()->servers[i].name.c_str(), TRANSPARENT, RGB(47, 53, 66), 0.7);
			LabelBoxLeft(70, sx + 25, 150, 10, (ConfigManager::Get()->servers[i].ip + ":" + ConfigManager::Get()->servers[i].port).c_str(), TRANSPARENT, RGB(116, 125, 140), 0.45);

			// Connect button
			if (FlatColorButton(5, sx + 2, 60, 32, "Connect", RGB(46, 213, 115), RGB(123, 237, 159), RGB(47, 53, 66)) 
				&& sessionManager->GetSessionState() == SS_NOT_CONNECTED)
			{
				mTmpWaitTimer = osGetTime();
				OverrideDialogTypeInfo();
				DrawDialogLoading("Connecting", "Make sure start server on your PC\nPlease be patient while app working", [=]()
				{
					SessionState state = sessionManager->ConnectToServer(&ConfigManager::Get()->servers[i]);

					if (osGetTime() - mTmpWaitTimer > (2 * TIME_SECOND))
					{
						if (state == SS_CONNECTED)
						{
							//TODO: fuck it current session manager way really mess around. we should really not do that.
							// maybe just 1 session 

							// Check when session was connected with server 
							return -1;
						}
						else
						{
							mDialogBoxCallLater = new PopupCallback([=]()
							{
								// We need set session manager back to not connected state
								// because of other state can't go there so it must be SS_FAILED
								sessionManager->SetSessionState(SS_NOT_CONNECTED);
								OverrideDialogTypeCritical();
								DrawDialogMessage(sessionManager, "Error", "Can't Connect to server!");
								return 0;
							});
							return -1;
						}
					}
					return 0;
				});
			}

			// Remove button
			if (FlatColorButton(286, sx + 6, 26, 26, "X", RGB(255, 71, 87), RGB(255, 107, 129), RGB(47, 53, 66)))
			{
				DrawDialogMessage(sessionManager, "Warning", "Are you sure to remove this profile?", [=]()
				{
					// on cancel
					return -1;
				}, [=]()
				{
					// on ok
					ConfigManager::Get()->servers.erase(ConfigManager::Get()->servers.begin() + i);
					ConfigManager::Get()->Save(); 
					return -1;
				});
			}
		}

	}else
	{
		// Draw empty screen and tutorial
		LabelBoxAutoWrap(10, 45, 300, 185, "No server profile found.\nPlease add new one by Add button.", TRANSPARENT, PPGraphics::Get()->PrimaryTextColor);
	}


	// Dialog box ( alway at bottom so it will draw on top )
	return DrawDialogBox(sessionManager);
}

int PPUI::DrawBtmAddNewServerProfileScreen(PPSessionManager* sessionManager, ResultCallback cancel, ResultCallback ok)
{
	PPGraphics::Get()->DrawRectangle(0, 0, 320, 240, RGB(236, 240, 241));
	PPGraphics::Get()->DrawRectangle(0, 0, 320, 25, RGB(26, 188, 156));

	// Screen title
	LabelBox(0, 5, 320, 20, "Add New Server", RGB(26, 188, 156), RGB(255, 255, 255));

	// Input server name
	LabelBoxLeft(5, 30, 100, 30, "Server Name", TRANSPARENT, RGB(44, 62, 80));
	if (LabelBox(105, 30, 210, 30, mTmpServerConfig->name.c_str(), PPGraphics::Get()->PrimaryColor, RGB(255, 255, 255)))
	{
		mTemplateInputString = std::string(mTmpServerConfig->name);
		DrawDialogKeyboard([=](void* a, void* b) {},
		[=](void* a, void* b)
		{
			// ok
			mTmpServerConfig->name.clear();
			mTmpServerConfig->name.append(mTemplateInputString);
			mTemplateInputString = "";
		});
	}

	// Input server ip
	LabelBoxLeft(5, 70, 100, 30, "Server IP", TRANSPARENT, RGB(44, 62, 80));
	if (LabelBox(105, 70, 210, 30, mTmpServerConfig->ip.c_str(), PPGraphics::Get()->PrimaryColor, RGB(255, 255, 255)))
	{
		mTemplateInputString = std::string(mTmpServerConfig->ip);
		DrawDialogNumberInput([=](void* a, void* b) {},
			[=](void* a, void* b)
		{
			// ok
			mTmpServerConfig->ip.clear();
			mTmpServerConfig->ip.append(mTemplateInputString);
			mTemplateInputString = "";
		});
	}


	// Input server port
	LabelBoxLeft(5, 110, 100, 30, "Server Port", TRANSPARENT, RGB(44, 62, 80));
	if (LabelBox(105, 110, 210, 30, mTmpServerConfig->port.c_str(), PPGraphics::Get()->PrimaryColor, RGB(255, 255, 255)))
	{
		mTemplateInputString = std::string(mTmpServerConfig->port);
		DrawDialogNumberInput([=](void* a, void* b) {},
			[=](void* a, void* b)
		{
			// ok
			mTmpServerConfig->port.clear();
			mTmpServerConfig->port.append(mTemplateInputString);
			mTemplateInputString = "";
		});
	}


	// Cancel button
	if (FlatColorButton(10, 200, 50, 30, "Cancel", RGB(192, 57, 43), RGB(231, 76, 60), RGB(223, 228, 234)))
	{
		ClosePopup();
		if (cancel != nullptr) cancel(nullptr, nullptr);
	}


	// OK button
	if (FlatColorButton(260, 200, 50, 30, "OK", RGB(41, 128, 185), RGB(52, 152, 219), RGB(223, 228, 234)))
	{
		mTmpWaitTimer = osGetTime();
		OverrideDialogTypeInfo();
		DrawDialogLoading("Testing connection", "Make sure start server on your PC\nPlease be patient while app working", [=]()
		{
			if (osGetTime() - mTmpWaitTimer > (2 * TIME_SECOND))
			{
				// return -1 for finish loading
				int ret = sessionManager->TestConnection(mTmpServerConfig);
				if (ret == 1)
				{
					mDialogBoxCallLater = new PopupCallback([=]()
					{
						OverrideDialogTypeSuccess();
						DrawDialogMessage(sessionManager, "Good Work", "Connection to server seem good!");

						ClosePopup();
						if (ok != nullptr) ok(nullptr, nullptr);

						return 0;
					});
					return -1;
				}
				else if (ret == -1)
				{
					mDialogBoxCallLater = new PopupCallback([=]()
					{
						OverrideDialogTypeCritical();
						DrawDialogMessage(sessionManager, "Error", "Can't Connect to server!");

						return 0;
					});
					return -1;
				}
			}
			return 0;
		});
	}

	// Dialog box ( alway at bottom so it will draw on top )
	return DrawDialogBox(sessionManager);
}


int PPUI::DrawBottomScreenUI(PPSessionManager* sessionManager)
{
//	PPGraphics::Get()->DrawRectangle(0, 0, 320, 240, RGB(236, 240, 241));
//	PPGraphics::Get()->DrawRectangle(0, 0, 320, 80, RGB(26, 188, 156));
//
//	// Screen title
//	switch (sessionManager->GetSessionState()) {
//	case -1: LabelBox(0, 0, 320, 20, "Status: No Wifi Connection", RGB(26, 188, 156), RGB(255, 255, 255)); break;
//	case 0: LabelBox(0, 0, 320, 20, "Status: Ready to Connect", RGB(26, 188, 156), RGB(255, 255, 255)); break;
//	case 1: LabelBox(0, 0, 320, 20, "Status: Connecting...", RGB(26, 188, 156), RGB(255, 255, 255)); break;
//	case 2: LabelBox(0, 0, 320, 20, "Status: Connected", RGB(26, 188, 156), RGB(255, 255, 255)); break;
//	}
//
//	// IP Port
//	LabelBox(20, 40, 230, 30, sessionManager->getIPAddress(), RGB(236, 240, 241), RGB(44, 62, 80));
//
//	// Edit Button
//	if (FlatColorButton(260, 40, 50, 30, "Edit", RGB(192, 57, 43), RGB(231, 76, 60), RGB(255, 255, 255)))
//	{
//		if (sessionManager->GetSessionState() == 2) return 0;
//
//		
//	}
//
//	// Tab Button
//
//	// Tab Content
//
//	if (FlatColorButton(260, 90, 50, 30, "Start", RGB(41, 128, 185), RGB(52, 152, 219), RGB(255, 255, 255)))
//	{
//		if (sessionManager->GetSessionState() == 2) return 0;
//		sessionManager->StartStreaming(sessionManager->getIPAddress());
//	}
//
//	// Sleep mode
//	if (FlatColorButton(200, 90, 50, 30, "Sleep", RGB(39, 174, 96), RGB(46, 204, 113), RGB(255, 255, 255)))
//	{
//		if (sleepModeState == 1) sleepModeState = 0;
//	}
//
//
//	// Config mode
//	if (FlatColorButton(10, 90, 120, 30, "> Adv. Config", RGB(39, 174, 96), RGB(46, 204, 113), RGB(255, 255, 255)))
//	{
//		AddPopup([=]()
//		{
//			return DrawStreamConfigUI(sessionManager,
//				[=](void* a, void* b)
//			{
//				// cancel
//			},
//				[=](void* a, void* b)
//			{
//				// ok
//				// save config
//				ConfigManager::Get()->Save();
//				// send new setting to server
//				sessionManager->UpdateStreamSetting();
//			}
//			);
//		});
//	}
//
//
//	// Exit Button
//	if (FlatColorButton(260, 200, 50, 30, "Exit", RGB(192, 57, 43), RGB(231, 76, 60), RGB(255, 255, 255)))
//	{
//		//DrawDialogMessage(sessionManager, "Warning!!!", "There is some shit in here There is some shit in here There is some shit in here There is some shit in here There is some shit in here ");
//		return -1;
//	}
//
//
//	InfoBox(sessionManager);
//
//
//	// Dialog box ( alway at bottom so it will draw on top )
//	DrawDialogBox(sessionManager);
	return 0;
}

void PPUI::OverrideDialogTypeWarning()
{
	mDialogOverride.isActivate = true;
	mDialogOverride.TitleBgColor = RGB(255, 127, 80);
	mDialogOverride.TitleTextColor = RGB(47, 53, 66);
}

void PPUI::OverrideDialogTypeInfo()
{
	mDialogOverride.isActivate = true;
	mDialogOverride.TitleBgColor = RGB(112, 161, 255);
	mDialogOverride.TitleTextColor = RGB(47, 53, 66);
}

void PPUI::OverrideDialogTypeSuccess()
{
	mDialogOverride.isActivate = true;
	mDialogOverride.TitleBgColor = RGB(123, 237, 159);
	mDialogOverride.TitleTextColor = RGB(47, 53, 66);
}

void PPUI::OverrideDialogTypeCritical()
{
	mDialogOverride.isActivate = true;
	mDialogOverride.TitleBgColor = RGB(255, 71, 87);
	mDialogOverride.TitleTextColor = RGB(47, 53, 66);
}

int PPUI::DrawDialogKeyboard(ResultCallback cancelCallback, ResultCallback okCallback)
{
	if (mDialogBox) return 0;
	mDialogBox = new PopupCallback([=]()
	{
		// draw background
		PPGraphics::Get()->DrawRectangle(0, 0, 320, 240, PPGraphics::Get()->TransBackgroundDark);

		// draw dialog box
		float boxY = 30;
		float boxHeight = 180;
		PPGraphics::Get()->DrawRectangle(13, boxY - 2, 294, boxHeight + 4, mDialogOverride.isActivate ? mDialogOverride.TitleBgColor : PPGraphics::Get()->PrimaryColor);
		PPGraphics::Get()->DrawRectangle(15, boxY, 290, boxHeight, RGB(247, 247, 247));

		// Input display
		LabelBox(22, boxY + 15, 276, 30, mTemplateInputString.c_str(), mDialogOverride.isActivate ? mDialogOverride.TitleBgColor : PPGraphics::Get()->PrimaryColor, RGB(247, 247, 247));

		// Keyboard Number
		int kW = 28;
		float startX = 22, startY = 60 + boxY;
		for (int c = 0; c < 10; c++)
		{
			for (int r = 0; r < 3; r++)
			{
				if (FlatButton(startX + c * kW, startY + r * kW, kW - 4, kW - 4, UI_KEYBOARD_VALUE[c + r * 10]))
				{
					char v = *UI_KEYBOARD_VALUE[c + r * 10];
					mTemplateInputString.push_back(v);
				}
			}
		}

		// Cancel button
		if (FlatColorButton(22, boxY + boxHeight - 30, 56, 25, "Cancel",
			PPGraphics::Get()->AccentColor, PPGraphics::Get()->AccentDarkColor, PPGraphics::Get()->AccentTextColor))
		{
			mTemplateInputString = "";
			cancelCallback(nullptr, nullptr);
			return -1;
		}

		// Delete Button
		if (FlatColorButton(192, boxY + boxHeight - 30, 56, 25, "Delete",
			PPGraphics::Get()->PrimaryColor, PPGraphics::Get()->PrimaryDarkColor, RGB(247, 247, 247)))
		{
			if (mTemplateInputString.size() > 0)
			{
				mTemplateInputString.erase(mTemplateInputString.end() - 1);
			}
		}

		// OK Button
		if (FlatColorButton(258, boxY + boxHeight - 30, 40, 25, "OK",
			PPGraphics::Get()->PrimaryColor, PPGraphics::Get()->PrimaryDarkColor, RGB(247, 247, 247)))
		{
			okCallback(nullptr, nullptr);
			return -1;
		}
		
		return 0;
	});
	return 0;
}

int PPUI::DrawDialogNumberInput(ResultCallback cancelCallback, ResultCallback okCallback)
{
	if (mDialogBox) return 0;
	mDialogBox = new PopupCallback([=]()
	{
		// draw background
		PPGraphics::Get()->DrawRectangle(0, 0, 320, 240, PPGraphics::Get()->TransBackgroundDark);

		// draw dialog box
		float boxY = 30;
		float boxHeight = 180;
		PPGraphics::Get()->DrawRectangle(13, boxY - 2, 294, boxHeight + 4, mDialogOverride.isActivate ? mDialogOverride.TitleBgColor : PPGraphics::Get()->PrimaryColor);
		PPGraphics::Get()->DrawRectangle(15, boxY, 290, boxHeight, RGB(247, 247, 247));

		// Input display
		LabelBox(22, boxY + 15, 276, 30, mTemplateInputString.c_str(), mDialogOverride.isActivate ? mDialogOverride.TitleBgColor : PPGraphics::Get()->PrimaryColor, RGB(247, 247, 247));

		// Keyboard Number
		int kW = 28;
		float startX = 22, startY = 60 + boxY;
		for (int c = 0; c < 3; c++)
		{
			for (int r = 0; r < 4; r++)
			{
				if (FlatButton(startX + c * kW, startY + r * kW, kW - 4, kW - 4, UI_INPUT_VALUE[c + r * 3]))
				{
					char v = *UI_INPUT_VALUE[c + r * 3];
					mTemplateInputString.push_back(v);
				}
			}
		}

		// Cancel button
		if (FlatColorButton(238, boxY + boxHeight - 30, 56, 25, "Cancel",
			PPGraphics::Get()->AccentColor, PPGraphics::Get()->AccentDarkColor, PPGraphics::Get()->AccentTextColor))
		{
			mTemplateInputString = "";
			cancelCallback(nullptr, nullptr);
			return -1;
		}

		// Delete Button
		if (FlatColorButton(192, startY, 56, 25, "Delete",
			PPGraphics::Get()->PrimaryColor, PPGraphics::Get()->PrimaryDarkColor, RGB(247, 247, 247)))
		{
			if (mTemplateInputString.size() > 0)
			{
				mTemplateInputString.erase(mTemplateInputString.end() - 1);
			}
		}

		// OK Button
		if (FlatColorButton(258, startY, 40, 25, "OK",
			PPGraphics::Get()->PrimaryColor, PPGraphics::Get()->PrimaryDarkColor, RGB(247, 247, 247)))
		{
			okCallback(nullptr, nullptr);
			return -1;
		}

		return 0;
	});
	return 0;
}


int PPUI::DrawDialogLoading(const char* title, const char* body, PopupCallback callback)
{
	if (mDialogBox) return 0;
	mTmpLoadingDirA = 1;
	mTmpLoadingDirB = 1;
	mTmpLoadingTimeA = osGetTime();
	mTmpLoadingTimeB = osGetTime();
	mDialogBox = new PopupCallback([=]()
	{
		ppVector3 bodySize = PPGraphics::Get()->GetTextSizeAutoWrap(body, 0.5, 0.5, 280);
		float popupHeight = bodySize.y + 40 + 36;
		if (popupHeight > 220) popupHeight = 220;

		float spaceY = (240.0f - popupHeight) / 2.0f;

		// draw background
		PPGraphics::Get()->DrawRectangle(0, 0, 320, 240, PPGraphics::Get()->TransBackgroundDark);

		// draw dialog box
		PPGraphics::Get()->DrawRectangle(13, spaceY, 294, popupHeight + 4, mDialogOverride.isActivate ? mDialogOverride.TitleBgColor : PPGraphics::Get()->PrimaryColor);
		PPGraphics::Get()->DrawRectangle(15, spaceY + 2, 290, popupHeight, RGB(247, 247, 247));

		// draw title
		LabelBox(15, spaceY + 2, 290, 30, title, mDialogOverride.isActivate ? mDialogOverride.TitleBgColor : PPGraphics::Get()->PrimaryColor, PPGraphics::Get()->PrimaryTextColor, 0.8);
		LabelBoxAutoWrap(20, spaceY + 40, 280, bodySize.y, body, RGB(255, 255, 255), PPGraphics::Get()->PrimaryTextColor);


		// animation part
		float minX = 60, maxX = 250;
		float minW = 10, maxW = 55;
		double duration = 1.0 * 1000ULL;
		double durationW = duration / 2.0;

		u64 timePassA = osGetTime() - mTmpLoadingTimeA;
		u64 timePassB = osGetTime() - mTmpLoadingTimeB;

		double p = (double)timePassA / duration;
		if (mTmpLoadingDirA == -1) p = 1.0 - p;
		double e = getEasingFunction(EaseInOutExpo)(p);
		float currentX = minX + e * (maxX - minX);

		double pW = (double)timePassB / durationW;
		if (mTmpLoadingDirB == -1) pW = 1.0 - pW;
		double eW = getEasingFunction(EaseInQuint)(pW);
		float currentW = minW + eW * (maxW - minW);

		PPGraphics::Get()->DrawRectangle(currentX, spaceY + 40 + bodySize.y + 4, currentW, 15, RGB(255, 165, 2));

		if (timePassA >= duration)
		{
			mTmpLoadingTimeA = osGetTime();
			mTmpLoadingDirA *= -1;
		}

		if (timePassB >= durationW)
		{
			mTmpLoadingTimeB = osGetTime();
			mTmpLoadingDirB *= -1;
		}

		return callback();
	});
	return 0;
}

int PPUI::DrawDialogMessage(PPSessionManager* sessionManager, const char* title, const char* body)
{
	if (mDialogBox) return 0;
	mDialogBox = new PopupCallback([=]()
	{
		ppVector3 bodySize = PPGraphics::Get()->GetTextSizeAutoWrap(body, 0.5, 0.5, 280);
		float popupHeight = bodySize.y + 40 + 36;
		if (popupHeight > 220) popupHeight = 220;

		float spaceY = (240.0f - popupHeight) / 2.0f;

		// draw background
		PPGraphics::Get()->DrawRectangle(0, 0, 320, 240, PPGraphics::Get()->TransBackgroundDark);

		// draw dialog box
		PPGraphics::Get()->DrawRectangle(13, spaceY, 294, popupHeight + 4, mDialogOverride.isActivate ? mDialogOverride.TitleBgColor : PPGraphics::Get()->PrimaryColor);
		PPGraphics::Get()->DrawRectangle(15, spaceY + 2, 290, popupHeight, RGB(247, 247, 247));

		// draw title
		LabelBox(15, spaceY + 2, 290, 30, title, mDialogOverride.isActivate ? mDialogOverride.TitleBgColor : PPGraphics::Get()->PrimaryColor, PPGraphics::Get()->PrimaryTextColor, 0.8);
		LabelBoxAutoWrap(20, spaceY + 40, 280, bodySize.y, body, RGB(255, 255, 255), PPGraphics::Get()->PrimaryTextColor);

		// draw button close
		if(FlatColorButton(135, spaceY + 40 + bodySize.y + 4, 50, 30, "Close", 
			PPGraphics::Get()->AccentColor, PPGraphics::Get()->AccentDarkColor, PPGraphics::Get()->AccentTextColor))
		{
			return -1;
		}
		return 0;
	});
	return 0;
}

int PPUI::DrawDialogMessage(PPSessionManager* sessionManager, const char* title, const char* body,
	PopupCallback closeCallback)
{
	if (mDialogBox) return 0;
	mDialogBox = new PopupCallback([=]()
	{
		ppVector3 bodySize = PPGraphics::Get()->GetTextSizeAutoWrap(body, 0.5, 0.5, 280);
		float popupHeight = bodySize.y + 40 + 36;
		if (popupHeight > 220) popupHeight = 220;

		float spaceY = (240.0f - popupHeight) / 2.0f;

		// draw background
		PPGraphics::Get()->DrawRectangle(0, 0, 320, 240, PPGraphics::Get()->TransBackgroundDark);

		// draw dialog box
		PPGraphics::Get()->DrawRectangle(13, spaceY, 294, popupHeight + 4, mDialogOverride.isActivate ? mDialogOverride.TitleBgColor : PPGraphics::Get()->PrimaryColor);
		PPGraphics::Get()->DrawRectangle(15, spaceY + 2, 290, popupHeight, RGB(247, 247, 247));

		// draw title
		LabelBox(15, spaceY + 2, 290, 30, title, mDialogOverride.isActivate ? mDialogOverride.TitleBgColor : PPGraphics::Get()->PrimaryColor, PPGraphics::Get()->PrimaryTextColor, 0.8);
		LabelBoxAutoWrap(20, spaceY + 40, 280, bodySize.y, body, RGB(255, 255, 255), PPGraphics::Get()->PrimaryTextColor);

		// draw button close
		if (FlatColorButton(135, spaceY + 40 + bodySize.y + 4, 50, 30, "Close",
			PPGraphics::Get()->AccentColor, PPGraphics::Get()->AccentDarkColor, PPGraphics::Get()->AccentTextColor))
		{
			if (closeCallback != nullptr) return closeCallback();
			return -1;
		}
		return 0;
	});
	return 0;
}

int PPUI::DrawDialogMessage(PPSessionManager* sessionManager, const char* title, const char* body,
	PopupCallback cancelCallback, PopupCallback okCallback)
{
	if (mDialogBox) return 0;
	mDialogBox = new PopupCallback([=]()
	{
		ppVector3 bodySize = PPGraphics::Get()->GetTextSizeAutoWrap(body, 0.5, 0.5, 280);
		float popupHeight = bodySize.y + 40 + 36;
		if (popupHeight > 220) popupHeight = 220;

		float spaceY = (240.0f - popupHeight) / 2.0f;

		// draw background
		PPGraphics::Get()->DrawRectangle(0, 0, 320, 240, PPGraphics::Get()->TransBackgroundDark);

		// draw dialog box
		PPGraphics::Get()->DrawRectangle(13, spaceY, 294, popupHeight + 4, mDialogOverride.isActivate ? mDialogOverride.TitleBgColor : PPGraphics::Get()->PrimaryColor);
		PPGraphics::Get()->DrawRectangle(15, spaceY + 2, 290, popupHeight, RGB(247, 247, 247));

		// draw title
		LabelBox(15, spaceY + 2, 290, 30, title, mDialogOverride.isActivate ? mDialogOverride.TitleBgColor : PPGraphics::Get()->PrimaryColor, PPGraphics::Get()->PrimaryTextColor, 0.8);
		LabelBoxAutoWrap(20, spaceY + 40, 280, bodySize.y, body, RGB(255, 255, 255), PPGraphics::Get()->PrimaryTextColor);

		// draw button close
		if (FlatColorButton(115, spaceY + 40 + bodySize.y + 4, 40, 30, "Close",
			PPGraphics::Get()->AccentColor, PPGraphics::Get()->AccentDarkColor, PPGraphics::Get()->AccentTextColor))
		{
			if (cancelCallback != nullptr) return cancelCallback();
			return -1;
		}

		// draw button ok
		if (FlatColorButton(165, spaceY + 40 + bodySize.y + 4, 40, 30, "OK",
			PPGraphics::Get()->PrimaryColor, PPGraphics::Get()->PrimaryDarkColor, PPGraphics::Get()->PrimaryTextColor))
		{
			if (okCallback != nullptr) return okCallback();
			return -1;
		}
		return 0;
	});
	return 0;
}

int PPUI::DrawStreamConfigUI(PPSessionManager* sessionManager, ResultCallback cancel, ResultCallback ok)
{
	PPGraphics::Get()->DrawRectangle(0, 0, 320, 240, RGB(236, 240, 241));
	LabelBox(0, 0, 320, 30, "Advance Config", RGB(26, 188, 156), RGB(255, 255, 255));


	
	//ConfigManager::Get()->_cfg_video_quality = Slide(5, 40, 300, 30, ConfigManager::Get()->_cfg_video_quality, 10, 100, "Quality");
	//ConfigManager::Get()->_cfg_video_scale = Slide(5, 70, 300, 30, ConfigManager::Get()->_cfg_video_scale, 10, 100, "Scale");
	//ConfigManager::Get()->_cfg_skip_frame = Slide(5, 100, 300, 30, ConfigManager::Get()->_cfg_skip_frame, 0, 60, "Skip Frame");
	//ConfigManager::Get()->_cfg_wait_for_received = ToggleBox(5, 130, 300, 30, ConfigManager::Get()->_cfg_wait_for_received, "Wait Received");


	// Cancel button
	if (FlatColorButton(200, 200, 50, 30, "Cancel", RGB(192, 57, 43), RGB(231, 76, 60), RGB(255, 255, 255)))
	{
		ClosePopup();
		if (cancel != nullptr) cancel(nullptr, nullptr);
	}

	// OK button
	if (FlatColorButton(260, 200, 50, 30, "OK", RGB(41, 128, 185), RGB(52, 152, 219), RGB(255, 255, 255)))
	{
		ClosePopup();
		if (ok != nullptr) ok(nullptr, nullptr);
	}
}

int PPUI::DrawIdleBottomScreen(PPSessionManager* sessionManager)
{
	// touch screen to wake up
	if(TouchUpOnArea(0,0, 320, 240))
	{
		sleepModeState = 1;
	}
	// label
	LabelBox(0, 0, 320, 240, "Touch screen to wake up", RGB(0, 0, 0), RGB(125, 125, 125));

	InfoBox(sessionManager);

	return 0;
}

void PPUI::InfoBox(PPSessionManager* sessionManager)
{
	// render video FPS
	char videoFpsBuffer[100];
	snprintf(videoFpsBuffer, sizeof videoFpsBuffer, "FPS:%.1f|VPS:%.1f", sessionManager->GetFPS(), sessionManager->GetVideoFPS());
	LabelBoxLeft(5, 220, 100, 20, videoFpsBuffer, TRANSPARENT, RGB(150, 150, 150), 0.4f);
	snprintf(videoFpsBuffer, sizeof videoFpsBuffer, "CPU:%.1f|GPU:%.1f|CMD:%.1f", C3D_GetProcessingTime()*6.0f, C3D_GetDrawingTime()*6.0f, C3D_GetCmdBufUsage()*100.0f);
	LabelBoxLeft(5, 210, 100, 20, videoFpsBuffer, TRANSPARENT, RGB(150, 150, 150), 0.4f);
}

int PPUI::DrawDialogBox(PPSessionManager* sessionManager)
{
	if (mDialogBox != nullptr) {
		mTmpLockTouch = false;
		int ret = (*mDialogBox)();
		mTmpLockTouch = true;
		if(ret < 0)
		{
			mTmpLockTouch = false;
			delete mDialogBox;
			mDialogBox = nullptr;
			// disable override after finish dialog
			mDialogOverride.isActivate = false;
			// trigger call later after finish dialog
			if(mDialogBoxCallLater != nullptr)
			{
				(*mDialogBoxCallLater)();
				delete mDialogBoxCallLater;
				mDialogBoxCallLater = nullptr;
			}

			if (ret == RET_CLOSE_APP) return -1;
		}
	}
	return 0;
}


///////////////////////////////////////////////////////////////////////////
// SLIDE
///////////////////////////////////////////////////////////////////////////

float PPUI::Slide(float x, float y, float w, float h, float val, float min, float max, float step, const char* label)
{
	ppVector2 tSize = PPGraphics::Get()->GetTextSize(label, 0.5f, 0.5f);
	float labelY = (h - tSize.y) / 2.0f;
	float labelX = x + 5.f;
	float slideX = w / 100.f * 35.f;
	float marginY = 2;

	if (val < min) val = min;
	if (val > max) val = max;
	// draw label
	PPGraphics::Get()->DrawText(label, x + labelX, y + labelY, 0.5f, 0.5f, RGB(26, 26, 26), false);

	// draw bg
	float startX = x + slideX;
	float startY = y + marginY;
	w = w - slideX;
	h = h - 2 * marginY;
	PPGraphics::Get()->DrawRectangle(startX, startY, w, h, PPGraphics::Get()->PrimaryDarkColor);
	
	char valBuffer[50];
	snprintf(valBuffer, sizeof valBuffer, "%.1f", val, val);
	ppVector2 valSize = PPGraphics::Get()->GetTextSize(valBuffer, 0.5f, 0.5f);
	float valueX = (w - valSize.x) / 2.0f;
	float valueY = (h - valSize.y) / 2.0f;

	// draw value
	PPGraphics::Get()->DrawText(valBuffer, startX + valueX, startY + valueY, 0.5f, 0.5f, PPGraphics::Get()->AccentTextColor, false);
	float newValue = val;
	// draw plus and minus button
	if(RepeatButton(startX + 1, startY + 1, 30 - 2, h - 2, "<", RGB(236, 240, 241), RGB(189, 195, 199), RGB(44, 62, 80)))
	{
		// minus
		newValue -= step;
	}

	if(RepeatButton(startX + w - 30, startY + 1, 30 - 1, h - 2, ">", RGB(236, 240, 241), RGB(189, 195, 199), RGB(44, 62, 80)))
	{
		// plus
		newValue += step;
	}
	if (newValue < min) newValue = min;
	if (newValue > max) newValue = max;
	return newValue;
}

///////////////////////////////////////////////////////////////////////////
// CHECKBOX
///////////////////////////////////////////////////////////////////////////
bool PPUI::ToggleBox(float x, float y, float w, float h, bool value, const char* label)
{
	ppVector2 tSize = PPGraphics::Get()->GetTextSize(label, 0.5f, 0.5f);
	float labelY = (h - tSize.y) / 2.0f;
	float labelX = x + 5.f;
	float boxSize = (w / 100.f * 35.f);
	float marginX = w - boxSize;
	float marginY = 2;

	// draw label
	PPGraphics::Get()->DrawText(label, x + labelX, y + labelY, 0.5f, 0.5f, RGB(26, 26, 26), false);

	// draw bg
	float startX = x + marginX;
	float startY = y + marginY;
	w = w - marginX;
	h = h - 2 * marginY;
	PPGraphics::Get()->DrawRectangle(startX, startY, w, h, PPGraphics::Get()->PrimaryDarkColor);

	bool result = value;
	if(value)
	{
		// on button
		FlatColorButton(startX + 1, startY + 1, (boxSize / 2) - 2, h - 2, "On", RGB(236, 240, 241), RGB(189, 195, 199), RGB(44, 62, 80));
		// off button
		if (FlatColorButton(startX + 1 + (boxSize / 2), startY + 1, (boxSize / 2) - 2, h - 2, "Off", PPGraphics::Get()->PrimaryDarkColor, PPGraphics::Get()->PrimaryColor, RGB(236, 240, 241)))
		{
			if(!mTmpLockTouch) result = false;
		}
	}else
	{
		// on button
		if (FlatColorButton(startX + 1, startY + 1, (boxSize / 2) - 2, h - 2, "On", PPGraphics::Get()->PrimaryDarkColor, PPGraphics::Get()->PrimaryColor, RGB(236, 240, 241)))
		{
			if (!mTmpLockTouch) result = true;
		}
		// off button
		FlatColorButton(startX + 1 + (boxSize / 2), startY + 1, (boxSize / 2) - 2, h - 2, "Off", RGB(236, 240, 241), RGB(189, 195, 199), RGB(44, 62, 80));
	}
	return result;
}

///////////////////////////////////////////////////////////////////////////
// BUTTON
///////////////////////////////////////////////////////////////////////////

bool PPUI::FlatButton(float x, float y, float w, float h, const char* label)
{
	return FlatColorButton(x, y, w, h, label, RGB(26, 188, 156), RGB(46, 204, 113), RGB(236, 240, 241));
}

bool PPUI::FlatDarkButton(float x, float y, float w, float h, const char* label)
{
	return FlatColorButton(x, y, w, h, label, RGB(22, 160, 133), RGB(39, 174, 96), RGB(236, 240, 241));
}

bool PPUI::FlatColorButton(float x, float y, float w, float h, const char* label, Color colNormal, Color colActive, Color txtCol)
{
	float tScale = 0.5f;
	if (TouchDownOnArea(x, y, w, h) && !mTmpLockTouch)
	{
		PPGraphics::Get()->DrawRectangle(x, y, w, h, colActive);
		tScale = 0.6f;
	}
	else
	{
		PPGraphics::Get()->DrawRectangle(x, y, w, h, colNormal);
	}
	ppVector2 tSize = PPGraphics::Get()->GetTextSize(label, tScale, tScale);
	PPGraphics::Get()->DrawText(label, x + (w - tSize.x) / 2.0f, y + (h - tSize.y) / 2.0f, tScale, tScale, txtCol, false);
	return TouchUpOnArea(x, y, w, h) && !mTmpLockTouch;
}

bool PPUI::RepeatButton(float x, float y, float w, float h, const char* label, Color colNormal, Color colActive, Color txtCol)
{
	bool isTouchDown = TouchDownOnArea(x, y, w, h);
	float tScale = 0.5f;
	u64 difTime = 0;
	if (isTouchDown && !mTmpLockTouch)
	{
		PPGraphics::Get()->DrawRectangle(x, y, w, h, colActive);
		tScale = 0.6f;
		
		if (holdTime == 0)
		{
			holdTime = osGetTime();
		}else
		{
			difTime = osGetTime() - holdTime;
		}
	}
	else
	{
		PPGraphics::Get()->DrawRectangle(x, y, w, h, colNormal);
	}
	ppVector2 tSize = PPGraphics::Get()->GetTextSize(label, tScale, tScale);
	float startX = (w - tSize.x) / 2.0f;
	float startY = (h - tSize.y) / 2.0f;
	PPGraphics::Get()->DrawText(label, x + startX, y + startY, tScale, tScale, txtCol, false);

	
	return (isTouchDown && difTime > 500 || TouchUpOnArea(x, y, w, h)) && !mTmpLockTouch;
}

///////////////////////////////////////////////////////////////////////////
// TEXT
///////////////////////////////////////////////////////////////////////////

/**
 * \brief Draw label box
 * \param x 
 * \param y 
 * \param w 
 * \param h 
 * \param defaultValue 
 * \param placeHolder 
 */
int PPUI::LabelBox(float x, float y, float w, float h, const char* label, Color bgColor, Color txtColor, float scale)
{
	PPGraphics::Get()->DrawRectangle(x, y, w, h, bgColor);
	ppVector2 tSize = PPGraphics::Get()->GetTextSize(label, scale, scale);
	float startX = (w - tSize.x) / 2.0f;
	float startY = (h - tSize.y) / 2.0f;
	PPGraphics::Get()->DrawText(label, x + startX, y + startY, scale, scale, txtColor, false);


#ifdef UI_DEBUG
	char buffer[100];
	snprintf(buffer, sizeof buffer, "w:%.02f|h:%.02f", tSize.x, tSize.y);
	PPGraphics::Get()->DrawText(buffer, x , y - 10, 0.8f * scale, 0.8f * scale, txtColor, false);
#endif

	return TouchUpOnArea(x, y, w, h) && !mTmpLockTouch;
}

int PPUI::LabelBoxAutoWrap(float x, float y, float w, float h, const char* label, Color bgColor, Color txtColor,
	float scale)
{
	PPGraphics::Get()->DrawRectangle(x, y, w, h, bgColor);
	ppVector3 tSize = PPGraphics::Get()->GetTextSizeAutoWrap(label, scale, scale, w);
	float startX = (w - tSize.x) / 2.0f;
	float startY = (h - tSize.y) / 2.0f;
	PPGraphics::Get()->DrawTextAutoWrap(label, x + startX, y + startY, w, scale, scale, txtColor, false);

#ifdef UI_DEBUG
	char buffer[100];
	snprintf(buffer, sizeof buffer, "w:%.02f|h:%.02f|l:%d", tSize.x, tSize.y, tSize.z);
	PPGraphics::Get()->DrawText(buffer, x, y - 10, 0.8f * scale, 0.8f * scale, txtColor, false);
#endif

	return TouchUpOnArea(x, y, w, h) && !mTmpLockTouch;
}

int PPUI::LabelBoxLeft(float x, float y, float w, float h, const char* label, Color bgColor, Color txtColor, float scale)
{
	PPGraphics::Get()->DrawRectangle(x, y, w, h, bgColor);
	ppVector2 tSize = PPGraphics::Get()->GetTextSize(label, scale, scale);
	float startY = (h - tSize.y) / 2.0f;
	PPGraphics::Get()->DrawText(label, x, y + startY, scale, scale, txtColor, false);

#ifdef UI_DEBUG
	char buffer[100];
	snprintf(buffer, sizeof buffer, "w:%.02f|h:%.02f", tSize.x, tSize.y);
	PPGraphics::Get()->DrawText(buffer, x, y - 10, 0.8f * scale, 0.8f * scale, txtColor, false);
#endif

	return TouchUpOnArea(x, y, w, h) && !mTmpLockTouch;
}


///////////////////////////////////////////////////////////////////////////
// POPUP
///////////////////////////////////////////////////////////////////////////

bool PPUI::HasPopup()
{
	return mPopupList.size() > 0;
}

PopupCallback PPUI::GetPopup()
{
	return mPopupList[mPopupList.size() - 1];
}

void PPUI::ClosePopup()
{
	mPopupList.erase(mPopupList.end() - 1);
}

void PPUI::AddPopup(PopupCallback callback)
{
	mPopupList.push_back(callback);
}