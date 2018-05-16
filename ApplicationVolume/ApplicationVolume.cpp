#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <windows.h>
#include <iostream>
#include <cstdio>
#include <Audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <TlHelp32.h>
#include <stdio.h>
#include <Shlwapi.h>
#include <comdef.h>
#include <atlstr.h> 

using namespace std;

/*
This program will allow you to change any applications volume.

It finds the ID for the application you want to change, then finds all the applications that show up in the mixer.
Compares those IDs to the yours and changes the volume.

With some easy modification this could be used to change the system Master Volume, there's a commented out line for that.
*/


DWORD MyGetProcessId(LPCTSTR);
bool ChangeVolume(float, DWORD);
float getVolumeAdd(float, float, float);

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
};

int main()
{
	/* must be from 0.0 to 1.0 */
	float volumeIncrease = .05;

	LPCTSTR program = "Chrome.exe";
	DWORD applicationID = MyGetProcessId(program);

	std::cout << applicationID << " pid" << std::endl;
	if (applicationID == 0) { printf("error 1"); getchar(); }//error

	//ChangeVolume(volumeIncrease, pid);


	while (ChangeVolume(volumeIncrease, applicationID))
	{
		/* So the increments can be observed */
		Sleep(250);
	}
	while (true);
	return 0;
}

//Lets you read HRESULT error messages
/*

com_error err(hr);
cout << err.ErrorMessage() << endl;

*/


/* Gets the process ID of the program to modify */
DWORD MyGetProcessId(LPCTSTR ProcessName) // non-conflicting function name
{
	PROCESSENTRY32 pt;
	HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	pt.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(hsnap, &pt)) { // must call this first
		do {
			if (!lstrcmpi(pt.szExeFile, ProcessName)) {
				CloseHandle(hsnap);
				return pt.th32ProcessID;
			}
		} while (Process32Next(hsnap, &pt));
	}
	CloseHandle(hsnap); // close handle on failure
	return 0;
}




bool ChangeVolume(float deltaVolume, DWORD pid)
{

	HRESULT                  hr;
	IMMDeviceEnumerator     *pEnumerator = NULL;
	ISimpleAudioVolume      *pVolume = NULL;
	IMMDevice               *pDevice = NULL;
	IAudioSessionManager2   *pManager = NULL;
	IAudioSessionEnumerator *pSessionEnumerator = NULL;
	int                      sessionCount = 0;
	float                      currentMasterVolume = NULL;

	/* MASTER VOLUME*************************************************/
	IAudioEndpointVolume    *pEndpoint = NULL;
	IMMDevice               *pDeviceMaster = NULL;
	/****************************************************************/



	// Get the device enumerator and initialize the application for COM
	//CoInitialize is a required call before CoCreateInstance
	CoInitialize(NULL);
	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);


	if (pEnumerator != NULL)
	{

		/* MASTER VOLUME*******************************************/
		hr = pEnumerator->GetDefaultAudioEndpoint(EDataFlow::eRender,
			ERole::eMultimedia, &pDeviceMaster);

		hr = pDeviceMaster->Activate(__uuidof(IAudioEndpointVolume),
			CLSCTX_ALL, NULL, (LPVOID *)&pEndpoint);
		pEndpoint->GetMasterVolumeLevelScalar(&currentMasterVolume);
		//cout << "Master Volume: " << currentMasterVolume << endl;
		//hr = pEndpoint->SetMasterVolumeLevelScalar((float)1.0, NULL);
		/**********************************************************/



		/* Get the session 2 manager*******************************/
		hr = pEnumerator->GetDefaultAudioEndpoint(EDataFlow::eRender,
			ERole::eMultimedia, &pDevice);

		hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
			NULL, (LPVOID *)&pManager);

		/* Get the session enumerator******************************/
		hr = pManager->GetSessionEnumerator(&pSessionEnumerator);

		/* Get the session count***********************************/
		hr = pSessionEnumerator->GetCount(&sessionCount);
	}


	/* Iterate through audio sessions(Applications) */
	for (int i = 0; i < sessionCount; i++)
	{
		IAudioSessionControl *ctrl = NULL;
		IAudioSessionControl2 *ctrl2 = NULL;
		DWORD processId = 0;

		/* Session handle for each session */
		hr = pSessionEnumerator->GetSession(i, &ctrl);

		if (FAILED(hr))
		{
			continue;
		}

		hr = ctrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctrl2);

		if (FAILED(hr))
		{
			SafeRelease(&ctrl);
			continue;
		}

		/* Process ID for iteration */
		hr = ctrl2->GetProcessId(&processId);

		if (FAILED(hr))
		{
			SafeRelease(&ctrl);
			SafeRelease(&ctrl2);
			continue;
		}
		/* Debugging */
		if (processId != pid)
		{
			//cout << "Session Count: " << sessionCount << endl;
			//cout << "          PID: " << pid << endl;
			//LPWSTR *str = NULL;
			//ctrl2->GetSessionIdentifier(&str);
			//cout << "    ProcessID: " << processId << endl << endl;
		}

		if (FAILED(hr))
		{
			SafeRelease(&ctrl);
			SafeRelease(&ctrl2);
			continue;
		}

		/* Checks if current process ID matches requested one */
		if (processId == pid)
		{
			hr = ctrl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVolume);

			if (FAILED(hr))
			{
				SafeRelease(&ctrl);
				SafeRelease(&ctrl2);
				continue;
			}

			/* Gets the process volume */
			float volume = NULL;
			hr = pVolume->GetMasterVolume(&volume);
			cout << "Volume: " << volume << endl;
			volume = volume * 1.05;

			/*Sets the process volume */
			float processVolume = getVolumeAdd(currentMasterVolume, volume, deltaVolume);
			hr = pVolume->SetMasterVolume(processVolume, NULL);
			hr = pVolume->GetMasterVolume(&volume);
			cout << "Volume End: " << volume << endl;
			cout << "--------------------------------------------------" << endl;

			/* Releasing all ctrl and ctrl2 pointers */

			/* Returns false if volume is at Max Volume 1 */
			if (FAILED(hr) || volume == 1)
			{
				//Error(hr, "Failed to set the master volume.");
				SafeRelease(&ctrl);
				SafeRelease(&ctrl2);
				SafeRelease(&pVolume);
				return FALSE;
			}
			/* Returning true if everything worked properly */
			SafeRelease(&ctrl);
			SafeRelease(&ctrl2);
			SafeRelease(&pVolume);
			return TRUE;
		}
		else
		{
			SafeRelease(&ctrl);
			SafeRelease(&ctrl2);
			continue;
		}
	}
	/* Return false if it never found the application */
	return FALSE;
}

float getVolumeAdd(float masterVolume, float processVolume, float deltaVolume)
{
	/*
	When you modify process volume it changes it on a scale from 0.0 to 1.0, 1.0 being whatever the current system volume is.
	-----
	-   -
	-   -
	-   -
	-***- - Master volume
	-   -
	-***- - Process Volume
	-   -
	-   -
	-   -
	-----
	Increasing process volume by 5% really only increases it by roughly 3% on this sound mixer. Because process volume
	is directly proportional to master volume.
	If master volume was at 100%, process would increase by 5%, master at 50%, process 2.5%.

	This function aims to do a flat % increase regardless what the Master Volume is currently At.
	*/

	//cout << " Master Volume: " << masterVolume << endl;
	//cout << "Process Volume: " << processVolume << endl;
	//cout << "  Delta Volume: " << deltaVolume << endl;
	float ratio = 1.0 / masterVolume;
	float add = deltaVolume * ratio;
	float deltaProcessVolume = processVolume + add;
	//cout << "        Ratio Volume: " << ratio << endl;
	//cout << "          Add Volume: " << add << endl;
	//cout << "Delta Process Volume: " << deltaProcessVolume << endl;
	if (deltaProcessVolume >= 1)
	{
		return 1.0;

	}
	else if (deltaProcessVolume == 0)
	{
		if (masterVolume == 0)
		{
			return 0;
		}
		else
		{
			deltaProcessVolume;
		}

	}
	else
	{
		return deltaProcessVolume;
	}
}