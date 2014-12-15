#include <tchar.h>
#include <Windows.h>
#include <string>
#include <iostream>
#include "resource.h"
#include <bass.h>
#include <basswasapi.h>

using namespace std;

typedef struct COLORIZATIONPARAMS
{
	COLORREF         clrColor;          //ColorizationColor
	COLORREF         clrAftGlow;	   //ColorizationAfterglow
	UINT             nIntensity;	   //ColorizationColorBalance -> 0-100
	UINT             clrAftGlowBal;    //ColorizationAfterglowBalance
	UINT		 clrBlurBal;       //ColorizationBlurBalance
	UINT		 clrGlassReflInt;  //ColorizationGlassReflectionIntensity
	BOOL             fOpaque;
}DWMColor;

DWMColor dwmcolor;
COLORREF Restore; // This will be used to restore back the colors to default

// Needed Function from DWM Library
HRESULT(WINAPI *DwmIsCompositionEnabled)(BOOL *pfEnabled);
HRESULT(WINAPI *DwmSetColorizationParameters) (COLORIZATIONPARAMS *colorparam, UINT unknown);
HRESULT(WINAPI *DwmGetColorizationParameters) (COLORIZATIONPARAMS *colorparam);

int stream;

// WASAPI function
DWORD CALLBACK WasapiProc(void *buffer, DWORD length, void *user)
{
	return TRUE; //We're not doing anything..
}

int main(int argc, char ** argv) {

	cout << "Loading dwmapi.dll.." << endl;
	HMODULE hDwmDLL = LoadLibrary(_T("dwmapi.dll")); // Loads the DWM DLL
	if (!hDwmDLL)
	{
		cout << "Unable to load DWM library!" << endl;
		return 1;
	}

	//Get functions
	*(FARPROC *)&DwmIsCompositionEnabled = GetProcAddress(hDwmDLL, "DwmIsCompositionEnabled");
	*(FARPROC *)&DwmGetColorizationParameters = GetProcAddress(hDwmDLL, (LPCSTR)127); //Function at index 127
	*(FARPROC *)&DwmSetColorizationParameters = GetProcAddress(hDwmDLL, (LPCSTR)131); //Function at index 131

	BOOL enabled;
	DwmIsCompositionEnabled(&enabled);

	if (!enabled) {
		cout << "Composition isn't enabled.." << endl;
		FreeLibrary(hDwmDLL);
		return 2;
	}

	if (!DwmGetColorizationParameters || !DwmSetColorizationParameters) {
		cout << "Failed to find API functions.." << endl;
		return 3;
	}

	cout << "Loaded!" << endl;

	DwmGetColorizationParameters(&dwmcolor);
	Restore = dwmcolor.clrColor;

	float r = GetRValue(dwmcolor.clrColor);
	float g = GetGValue(dwmcolor.clrColor);
	float b = GetBValue(dwmcolor.clrColor);

	cout << "Looking for loopback device.." << endl;
	// find the loopback device for the default output
	int devnum = -1;
	BASS_WASAPI_DEVICEINFO info;
	for (int a = 0; BASS_WASAPI_GetDeviceInfo(a, &info); a++) {
		if (!(info.flags&BASS_DEVICE_INPUT) // found an output device (not input)
			&& (info.flags&BASS_DEVICE_DEFAULT)) { // and it is the default
			devnum = a + 1; // use it (+1 because the next device is the corresponding loopback device)
			break;
		}
	}

	cout << "Device #: " << devnum << endl;

	cout << "Loading libbass.." << endl;
	BASS_Init(0, 44100, 0, NULL, NULL); //Because of use of BASS_WASAPI_BUFFER


	if (devnum >= 0)
	{
		cout << "Loading WASAPI libbass add-on.." << endl;
		if (!BASS_WASAPI_Init(devnum, 0, 0, BASS_WASAPI_BUFFER, 1.0, 0, WasapiProc, NULL))
		{
			cout << "Error hooking into WASAPI" << endl;
			return 4;
		}

		BASS_WASAPI_Start();
	}

	Sleep(1000);

	cout << "Let's rave!" << endl;

	while (true) {
		float fft[1024] = { 0 };
		BASS_WASAPI_GetData(fft, BASS_DATA_FFT2048); //Get data
		float num = 0;
		for (int i = 0; i <= 5; i++) {
			num += fft[i];
		}
		num *= (13.0f / 15.0f);

		if (num < 0) {
			num = 0;
		}
		float mult = num / 1.5f;
		long l = RGB(min(r * mult, 255), min(g * mult, 255), min(b * mult, 255));

		dwmcolor.clrColor = l;
		DwmSetColorizationParameters(&dwmcolor, 0);

		Sleep(50);
	}
}