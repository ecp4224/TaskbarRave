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

void rgb2hsv(float r, float g, float b, float &h, float &s, float &v) {
	float K = 0.f;

	if (g < b)
	{
		swap(g, b);
		K = -1.f;
	}

	if (r < g)
	{
		swap(r, g);
		K = -2.f / 6.f - K;
	}

	float chroma = r - min(g, b);
	h = fabs(K + (g - b) / (6.f * chroma + 1e-20f));
	s = chroma / (r + 1e-20f);
	v = r;
}

long calculateNewRGB(float h, float s, float v, float num) {

	num = num / 1.5;

	v = (255.f * num);

	float r, g, b;

	int i;
	float f, p, q, t;
	if (s == 0) {
		// achromatic (grey)
		r = g = b = v;
		return RGB(r, g, b);
	}
	h /= 60;			// sector 0 to 5
	i = floor(h);
	f = h - i;			// factorial part of h
	p = v * (1 - s);
	q = v * (1 - s * f);
	t = v * (1 - s * (1 - f));
	switch (i) {
	case 0:
		r = v;
		g = t;
		b = p;
		break;
	case 1:
		r = q;
		g = v;
		b = p;
		break;
	case 2:
		r = p;
		g = v;
		b = t;
		break;
	case 3:
		r = p;
		g = q;
		b = v;
		break;
	case 4:
		r = t;
		g = p;
		b = v;
		break;
	default:		// case 5:
		r = v;
		g = p;
		b = q;
		break;
	}

	return RGB(r, g, b);
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

	float h, s, v;
	rgb2hsv(r, g, b, h, s, v);

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

		//cout << num << endl;

		dwmcolor.clrColor = l;
		DwmSetColorizationParameters(&dwmcolor, 0);

		Sleep(50);
	}
}