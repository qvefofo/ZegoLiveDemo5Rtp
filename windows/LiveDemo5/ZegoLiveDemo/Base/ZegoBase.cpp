#include "stdafx.h"
#include "ZegoBase.h"
#include "ZegoUtility.h"
#include "ZegoSigslotDefine.h"
#include "LiveRoom.h"
#include "LiveRoom-IM.h"
#include "LiveRoom-Player.h"
#include "LiveRoom-Publisher.h"
#include "LiveRoomDefines-Publisher.h"
#include "zego_sdk_protocol.h"


#if defined ZEGO_PROTOCOL_UDP

static std::string g_strAppName = "Live5-UDP";
static DWORD g_dwAppID2 = 1739272706;
static BYTE g_bufSignKey2[] =
{
    0x1e, 0xc3, 0xf8, 0x5c, 0xb2, 0xf2, 0x13, 0x70,
    0x26, 0x4e, 0xb3, 0x71, 0xc8, 0xc6, 0x5c, 0xa3,
    0x7f, 0xa3, 0x3b, 0x9d, 0xef, 0xef, 0x2a, 0x85,
    0xe0, 0xc8, 0x99, 0xae, 0x82, 0xc0, 0xf6, 0xf8
};

#elif defined ZEGO_PROTOCOL_UDP_INTERNATIONAL

m_bIsGlobalVersion = true;
static std::string g_strAppName = "Live5-INTERNATIONAL";
static DWORD g_dwAppID2 = 3322882036;
static BYTE g_bufSignKey2[] =
{ 
    0x5d, 0xe6, 0x83, 0xac, 0xa4, 0xe5, 0xad, 0x43,
    0xe5, 0xea, 0xe3, 0x70, 0x6b, 0xe0, 0x77, 0xa4,
    0x18, 0x79, 0x38, 0x31, 0x2e, 0xcc, 0x17, 0x19,
    0x32, 0xd2, 0xfe, 0x22, 0x5b, 0x6b, 0x2b, 0x2f 
};
    
#else

static std::string g_strAppName = "LiveDemo5";
static DWORD g_dwAppID2 = 1;
static BYTE g_bufSignKey2[] =
{
    0x91, 0x93, 0xcc, 0x66, 0x2a, 0x1c, 0x0e, 0xc1,
    0x35, 0xec, 0x71, 0xfb, 0x07, 0x19, 0x4b, 0x38,
    0x41, 0xd4, 0xad, 0x83, 0x78, 0xf2, 0x59, 0x90,
    0xe0, 0xa4, 0x0c, 0x7f, 0xf4, 0x28, 0x41, 0xf7
};

#endif


LRESULT CALLBACK ZegoCommuExchangeWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_ZEGO_SWITCH_THREAD)
	{
		std::function<void(void)>* pFunc = (std::function<void(void)>*)wParam;
		(*pFunc)();
		delete pFunc;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

std::string CZegoBase::GetAppName()
{
    return g_strAppName;
}

bool CZegoBase::IsTestEnv()
{
    return m_bIsTestEnv;
}

bool CZegoBase::IsGlobalVersion()
{
    return m_bIsGlobalVersion;
}

CZegoBase::CZegoBase(void) : m_dwInitedMask(INIT_NONE)
{
	WCHAR szAppName[MAX_PATH] = {0};
	::GetModuleFileNameW(NULL, szAppName, MAX_PATH);
	CString strAppFullName = szAppName;
	CString strPath = strAppFullName.Left(strAppFullName.ReverseFind('\\') + 1);
	m_strLogPathUTF8 = WStringToUTF8(strPath);

	// 创建隐藏的通信窗体
	WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
	wcex.hInstance = GetModuleHandle(0);
	wcex.lpszClassName = ZegoCommWndClassName;
	wcex.lpfnWndProc = &ZegoCommuExchangeWndProc;
	wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	RegisterClassEx(&wcex);
	m_hCommuWnd = CreateWindowEx(WS_EX_TOOLWINDOW, wcex.lpszClassName, ZegoCommWndName, WS_POPUP, 0, 0, 100, 100,
		NULL, NULL, wcex.hInstance, NULL);
	ShowWindow(m_hCommuWnd, SW_HIDE);

	m_pAVSignal = new CZegoAVSignal;
}

CZegoBase::~CZegoBase(void)
{
	UninitAVSdk();

	delete m_pAVSignal;

	DestroyWindow(m_hCommuWnd);
	CloseWindow(m_hCommuWnd);
}

bool CZegoBase::InitAVSdk(SettingsPtr pCurSetting, std::string userID, std::string userName)
{
	if (!IsAVSdkInited())
	{
        LIVEROOM::SetLogDir(m_strLogPathUTF8.c_str());
        LIVEROOM::SetBusinessType(0);
        LIVEROOM::SetUser(userID.c_str(), userName.c_str());
        // ToDo: 需要通过代码获取网络类型
        LIVEROOM::SetNetType(2);

        AVE::ExtPrepSet set;
        set.bEncode = false;
        set.nChannel = 0;
        set.nSampleRate = 0;
        set.nSamples = 0;

        LIVEROOM::SetAudioPrep2(&OnPrepData, set);

		LIVEROOM::InitSDK(g_dwAppID2, g_bufSignKey2, 32);

#if defined(ZEGO_PROTOCOL_UDP) || defined(ZEGO_PROTOCOL_UDP_INTERNATIONAL)
        LIVEROOM::EnableTrafficControl(AV::ZEGO_TRAFFIC_FPS | AV::ZEGO_TRAFFIC_RESOLUTION, true);
#endif
        LIVEROOM::SetLivePublisherCallback(m_pAVSignal);
        LIVEROOM::SetLivePlayerCallback(m_pAVSignal);
        LIVEROOM::SetRoomCallback(m_pAVSignal);
        LIVEROOM::SetIMCallback(m_pAVSignal);
        LIVEROOM::SetDeviceStateCallback(m_pAVSignal);
	}

    LIVEROOM::EnableAux(false);
    LIVEROOM::SetPlayVolume(100);
	if (!pCurSetting->GetMircophoneId().empty())
	{
        LIVEROOM::SetAudioDevice(AV::AudioDevice_Input, pCurSetting->GetMircophoneId().c_str());
	}

    LIVEROOM::SetVideoCaptureResolution(pCurSetting->GetResolution().cx, pCurSetting->GetResolution().cy);
    LIVEROOM::SetVideoEncodeResolution(pCurSetting->GetResolution().cx, pCurSetting->GetResolution().cy);
    LIVEROOM::SetVideoBitrate(pCurSetting->GetBitrate());
    LIVEROOM::SetVideoFPS(pCurSetting->GetFps());
	if (!pCurSetting->GetCameraId().empty())
	{
        LIVEROOM::SetVideoDevice(pCurSetting->GetCameraId().c_str());
	}

	m_dwInitedMask |= INIT_AVSDK;
	return true;
}

void CZegoBase::UninitAVSdk(void)
{
	if (IsAVSdkInited())
	{
        LIVEROOM::SetLivePublisherCallback(nullptr);
        LIVEROOM::SetLivePlayerCallback(nullptr);
        LIVEROOM::SetRoomCallback(nullptr);
        LIVEROOM::SetIMCallback(nullptr);
        LIVEROOM::SetDeviceStateCallback(nullptr);

        LIVEROOM::UnInitSDK();

		DWORD dwNegation = ~(DWORD)INIT_AVSDK;
		m_dwInitedMask &= dwNegation;
	}
}

bool CZegoBase::IsAVSdkInited(void)
{
	return (m_dwInitedMask & INIT_AVSDK) == INIT_AVSDK;
}

CZegoAVSignal& CZegoBase::GetAVSignal(void)
{
	return *m_pAVSignal;
}

DWORD CZegoBase::GetAppID(void)
{
    return g_dwAppID2;
}

void CZegoBase::OnPrepData(const AVE::AudioFrame& inFrame, AVE::AudioFrame& outFrame)
{
    outFrame.frameType = inFrame.frameType;
    outFrame.samples = inFrame.samples;
    outFrame.bytesPerSample = inFrame.bytesPerSample;
    outFrame.channels = inFrame.channels;
    outFrame.sampleRate = inFrame.sampleRate;
    outFrame.timeStamp = inFrame.timeStamp;
    outFrame.configLen = inFrame.configLen;
    outFrame.bufLen = inFrame.bufLen;
    memcpy(outFrame.buffer, inFrame.buffer, inFrame.bufLen);
}