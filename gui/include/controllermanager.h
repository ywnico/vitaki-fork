// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_CONTROLLERMANAGER_H
#define CHIAKI_CONTROLLERMANAGER_H

#include <chiaki/controller.h>

#include <QObject>
#include <QSet>
#include <QMap>
#include <QString>

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
#include <SDL.h>
#include <chiaki/orientation.h>
#endif

#define PS_TOUCHPAD_MAX_X 1920
#define PS_TOUCHPAD_MAX_Y 1079

class Controller;

class ControllerManager : public QObject
{
	Q_OBJECT

	friend class Controller;

	private:
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
		QSet<SDL_JoystickID> available_controllers;
#endif
		QMap<int, Controller *> open_controllers;

		void ControllerClosed(Controller *controller);

	private slots:
		void UpdateAvailableControllers();
		void HandleEvents();
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
		void ControllerEvent(SDL_Event evt);
#endif

	public:
		static ControllerManager *GetInstance();

		ControllerManager(QObject *parent = nullptr);
		~ControllerManager();

		QSet<int> GetAvailableControllers();
		Controller *OpenController(int device_id);

	signals:
		void AvailableControllersUpdated();
};

class Controller : public QObject
{
	Q_OBJECT

	friend class ControllerManager;

	private:
		Controller(int device_id, ControllerManager *manager);

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
		void UpdateState(SDL_Event event);
		bool HandleButtonEvent(SDL_ControllerButtonEvent event);
		bool HandleAxisEvent(SDL_ControllerAxisEvent event);
#if SDL_VERSION_ATLEAST(2, 0, 14)
		bool HandleSensorEvent(SDL_ControllerSensorEvent event);
		bool HandleTouchpadEvent(SDL_ControllerTouchpadEvent event);
#endif
#endif

		ControllerManager *manager;
		int id;
		ChiakiOrientationTracker orientation_tracker;
		ChiakiControllerState state;

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
		QMap<QPair<Sint64, Sint64>, uint8_t> touch_ids;
		SDL_GameController *controller;
#endif

	public:
		~Controller();

		bool IsConnected();
		int GetDeviceID();
		QString GetName();
		ChiakiControllerState GetState();
		void SetRumble(uint8_t left, uint8_t right);

	signals:
		void StateChanged();
};

#endif // CHIAKI_CONTROLLERMANAGER_H
