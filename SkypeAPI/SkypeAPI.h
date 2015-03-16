// SkypeAPI.h: interface for the CSkypeAPI class.
#if !defined(AFX_SKYPEAPI_H__3HNDSLSS_ECD9_44552_A957_9DFGDS15CB4__INCLGSGFSUDED_)
#define AFX_SKYPEAPI_H__3HNDSLSS_ECD9_44552_A957_9DFGDS15CB4__INCLGSGFSUDED_

#if _MSC_VER > 1000
#pragma once
#endif 
#define GREETINGS \
" ---------------------\r\n\
| <SkypeApplication>  |\r\n\
| ------------------- |\r\n\
| ���ڣ�2007��04��18��|\r\n\
 ---------------------\r\n\
"

typedef void (*Log_FUNC)(LPCTSTR lpszText, ... );    //����Enum�������ͳ��� 
enum
{
	SKYPECONTROLAPI_ATTACH_SUCCESS=0,
	SKYPECONTROLAPI_ATTACH_PENDING_AUTHORIZATION=1,
	SKYPECONTROLAPI_ATTACH_REFUSED=2,
	SKYPECONTROLAPI_ATTACH_NOT_AVAILABLE=3,
	SKYPECONTROLAPI_ATTACH_API_AVAILABLE=0x8001,
};

enum
{
	WM_SKYPEMSG_USERS = WM_USER+1000,
};

class CSkypeAPI                           //CHwSkype���ǳ������Ҫ����  
{
public:
	BOOL KeyPressd ( LPCTSTR lpszKey );
	void EndCurrentCall();
	BOOL SendComToSkype ( LPCTSTR lpszMsg, ... );
	BOOL IsSkypeWindowOK();
	BOOL TranslateMessage ( UINT message, WPARAM wParam, LPARAM lParam );
	CSkypeAPI( Log_FUNC Log_In );
	virtual ~CSkypeAPI();
	BOOL InitialConnection ( HWND hWnd_MainWindow );

private:
	CString m_csCallStatus;
	DWORD m_dwDialID;
	BOOL SkypeAPISyntaxInterpret ( LPCTSTR lpszMsg );
	Log_FUNC InsertMessage;
	HWND m_hWnd_Application;      //���������ھ��
	HWND m_hWnd_Skype;           //Skype������
	UINT m_MsgID_SkypeAttach;
	UINT m_MsgID_SkypeDiscover;
};

#endif // !defined(AFX_SKYPEAPI_H__3HNDSLSS_ECD9_44552_A957_9DFGDS15CB4__INCLGSGFSUDED_)
