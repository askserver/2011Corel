// SkypeAPI.cpp: implementation file
#include "stdafx.h"
#include "SkypeAPIApplication.h"
#include "SkypeAPI.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// ���ַ����ֿ�����ӵ��ַ��������У��罫�ַ�����aaa\nbbb\nccc���ֿ�Ϊ��
// ��aaa������bbb������ccc����ӵ� StrAry ��
// ע�⣺���������ʹ���� strtok() C�⺯����strtok() �����ǲ���Ƕ��ʹ�õ�
// ���ԣ�����ڵ��� PartStringAndAddToStrAry() ���ϲ�������Ѿ�ʹ���� strtok()
// ��ô�����ܵ��ñ����� PartStringAndAddToStrAry(),���Ե��� PartStringAndAddToStrAry()
// ����һ�汾, ���Ǽ򵥵�ͨ�� strchr() ������ɵ�.
//
int PutMSGToStrAry ( char *pStr, CStringArray &StrAry, char *seps/*="\t\r\n"*/ )
{
	StrAry.RemoveAll();
	char *token;
	token = strtok( pStr, seps );
	while( token != NULL )
	{
		StrAry.Add ( token );
		token = strtok( NULL, seps );
	}
	return StrAry.GetSize();
}

CSkypeAPI::CSkypeAPI ( Log_FUNC Log_In ): InsertMessage ( Log_In ), m_hWnd_Application ( NULL ), m_hWnd_Skype ( NULL ), m_MsgID_SkypeAttach ( 0 ), m_MsgID_SkypeDiscover ( 0 ), m_dwDialID ( 0 )
{
	ASSERT ( InsertMessage );
}

CSkypeAPI::~CSkypeAPI()
{

}

BOOL CSkypeAPI::InitialConnection ( HWND hWnd_MainWindow )           //CHwSkype�ĳ�ʼ������
{
	m_hWnd_Application = hWnd_MainWindow;
	ASSERT ( ::IsWindow ( m_hWnd_Application ) );       //�жϵ�ǰ���ھ���Ƿ���Ч
	m_MsgID_SkypeAttach = RegisterWindowMessage("SkypeControlAPIAttach");          //ע���Զ�����Ϣ
	m_MsgID_SkypeDiscover = RegisterWindowMessage("SkypeControlAPIDiscover");      //ע���Զ�����Ϣ
	if ( m_MsgID_SkypeAttach==0 || m_MsgID_SkypeDiscover==0 )
	{
		return FALSE;                   //���û�з���Skype�������û�����ӵ�Skype���򷵻�ʧ��
	}
	
	SendMessage( HWND_BROADCAST, m_MsgID_SkypeDiscover, (WPARAM)m_hWnd_Application, 0);  //��ǰǰ̨����(Skype)����m_MsgID_SkypeDiscover��Ϣ������Ϣ����TranlateMessage���Ͳ�Ѱ�ҵ�������
	return TRUE;
}

BOOL CSkypeAPI::IsSkypeWindowOK()
{
	return ( ::IsWindow ( m_hWnd_Skype ) );                //m_hWnd_Skype��Skype�������ھ��������ر������������ᱨ����ʹ��С��������Ҳ���У�
}

BOOL CSkypeAPI::SendComToSkype ( LPCTSTR lpszMsg, ... )           //...��ʾ���������Ϊ���LPCTSTR���͵Ĳ�������Щ����ͨ��va_list���������ݵ�
{
	if ( !lpszMsg || strlen(lpszMsg) < 1 ) return FALSE;
	if ( !IsSkypeWindowOK() ) return FALSE;                       //���Skype�����ھ�������ڣ��򷵻�ʧ��

	COPYDATASTRUCT CopyData = {0};                         
	
	//��Win32�У�WM_COPYDATA��Ϣ��ҪĿ���������ڽ��̼䴫��ֻ�����ݡ�
	//SDK�ĵ��Ƽ��û�ʹ��SendMessage()���������շ������ݸ������ǰ�����أ�
	//�������ͷ��Ͳ�����ɾ�����޸����ݡ�
	//���������ԭ��Ϊ��SendMessage(WM_COPYDATA,wParam,lParam)
	//����wParam����Ϊ�������ݵĴ��ھ����lParamָ��һ��COPYDATASTRUCT�Ľṹ,
	//COPYDATASTRUCT�ṹ�У�dwDataΪ�Զ������ݣ�cbDataΪ���ݴ�С��lpDataΪָ�����ݵ�ָ��
	
	char buf[1024] = {0};
	
	va_list  va;
	va_start ( va, lpszMsg );
	_vsnprintf  ( buf, sizeof(buf) - 1, (char*)lpszMsg, va);
	va_end(va);

	CopyData.dwData = 0;
	CopyData.lpData = (PVOID)buf;
	CopyData.cbData = strlen(buf)+1;
	if ( SendMessage ( m_hWnd_Skype, WM_COPYDATA, WPARAM(m_hWnd_Application),LPARAM(&CopyData)) == 0 )  //��Skype��������Ϣ
	{
		InsertMessage ( "Send message [%s] failed", lpszMsg );                //SendMessage��������Ϣ���ͳ�ȥ��Ҫ��ϵͳ����ظ������û�гɹ��򷵻�ʧ��
		return FALSE;
	}
	return TRUE;       //������ͳɹ�����Ϣ����Skypeȥ�������سɹ�
}

BOOL CSkypeAPI::SkypeAPISyntaxInterpret(LPCTSTR lpszMsg)          //SkypeAPI�﷨���ͺ���
{
	if ( !lpszMsg || strlen(lpszMsg) < 1 )             //�����Ϣ����Ϊ0������ϢΪFalse������ʧ��
		return FALSE;
	ASSERT ( ::IsWindow(m_hWnd_Application) && IsSkypeWindowOK() );  //��鱾�����Skype��ǰ���ھ���Ƿ���Ч

	CStringArray StrAry;                                     //��CStringArray���ʹ洢��Ϣ�ĸ��������������������
	int nStrNum = PutMSGToStrAry ( (char*)lpszMsg, StrAry, " ,;\t" );
	if ( nStrNum < 1 ) return FALSE;
	int nCmdPos = 0;
	// �����б�
	if ( StrAry.GetAt (nCmdPos) == "USERS" )              //�����Ϣ�ĵ�һ���ؼ���ΪUSERS���򽫸���Ϣ���͸�������
	{
		nCmdPos ++;
		SendMessage ( m_hWnd_Application, WM_SKYPEMSG_USERS, WPARAM(&StrAry),LPARAM(nCmdPos) );
	}
	// ���з��ؽ��
	else if ( StrAry.GetAt (nCmdPos) == "CALL" )       //�����Ϣ�ĵ�һ���ؼ���ΪCALL�����������1ȡ��һ���ؼ���
	{
		nCmdPos ++;
		if ( StrAry.GetSize() > nCmdPos )
		{
			m_dwDialID = (DWORD) atoi ( StrAry.GetAt(nCmdPos) );   //ȡ�ò���ĺ���
			nCmdPos ++;
			// ����״̬
			if ( StrAry.GetSize() > nCmdPos && StrAry.GetAt (nCmdPos) == "STATUS" )    //����������ؼ���ΪSTATUS
			{
				nCmdPos ++;
				if ( StrAry.GetSize() > nCmdPos )
				{
					m_csCallStatus = StrAry.GetAt (nCmdPos);       //ȡ�ò����״̬
					// ���н���
					if ( m_csCallStatus == "FAILED" || m_csCallStatus == "MISSED" )    //������ĸ��ؼ���ΪFAILED����MISSED  
					{
						m_dwDialID = 0;                            //������ĺ���ظ�Ϊ0
					}
				}
			}
			// SUBJECT
			else if ( StrAry.GetSize() > nCmdPos && StrAry.GetAt (nCmdPos) == "SUBJECT" )//����������ؼ���ΪSUBJECT����������
			{
			}
		}
	}
	StrAry.RemoveAll ();           //�����ʱ��Ϣ����

	return TRUE;
}

void CSkypeAPI::EndCurrentCall()      // ������ǰͨ��
{
	if ( m_dwDialID < 1 ) return;    //����������Ϊ0����ֱ�ӷ��سɹ�������ͨ����������������Skype������Ϣ��ֹͨ��
	SendComToSkype ( "SET CALL %u STATUS FINISHED", m_dwDialID );
}

BOOL CSkypeAPI::TranslateMessage ( UINT message, WPARAM wParam, LPARAM lParam )   //��������Ϣ���ͺ���
{
	if ( message == WM_COPYDATA && m_hWnd_Skype == (HWND)wParam )  //�����Ϣ����Skype���͹�������ʾ��Ϣ���ݣ���������
	{
		PCOPYDATASTRUCT poCopyData = (PCOPYDATASTRUCT)lParam;
		InsertMessage ( "<<�� %.*s\n", poCopyData->cbData, poCopyData->lpData); 
		SkypeAPISyntaxInterpret ( LPCTSTR(poCopyData->lpData) );
		return TRUE;
	}
	
	if ( message != m_MsgID_SkypeAttach && message != m_MsgID_SkypeDiscover )  //�����Ϣ����Skype���͵���Ϣ�����Ҳ������ӵ�Skype����û�а�װSkype�����򷵻�ʧ��
	{
		return FALSE;
	}

	if ( message == m_MsgID_SkypeAttach )                        //���������Skype����Ϣ
	{
		switch ( lParam )
		{
		case SKYPECONTROLAPI_ATTACH_SUCCESS:                     //�����Skype���ӳɹ���
			{
				InsertMessage ("<<�� Connected to Skype!;" );
				m_hWnd_Skype=(HWND)wParam;
				ASSERT ( ::IsWindow ( m_hWnd_Skype ) );          //�жϵ�ǰSkype���ھ���Ƿ���Ч
				break;
			}
		case SKYPECONTROLAPI_ATTACH_PENDING_AUTHORIZATION:       //�����Skype���ӳɹ���
			InsertMessage ("<<�� Waiting Skype authorization!" );
			break;
		case SKYPECONTROLAPI_ATTACH_REFUSED:                     //�����Skype������δ�õ���Ȩ
			InsertMessage ("<<�� Connection to Skype has been refused!" );
			break;
		case SKYPECONTROLAPI_ATTACH_NOT_AVAILABLE:               //�������SkypeAPI��Ч
			InsertMessage ("<<�� Skype API not available!" );
			break;
		case SKYPECONTROLAPI_ATTACH_API_AVAILABLE:               //���SkypeAPI��Ч
			InsertMessage ("<<�� Try to connect Skype API now !" );
			break;
		}
	}
	else if ( message == m_MsgID_SkypeDiscover )                //����յ�����Skype�����ֵ���Ϣ
	{
		InsertMessage ( "<<�� Skype Client Discovered!" );
	}
	
	return TRUE;
}

BOOL CSkypeAPI::KeyPressd(LPCTSTR lpszKey)  // ���Ͱ�����Ϣ
{
	if ( !IsSkypeWindowOK() ) return FALSE;                           //���δ���Skyoe��ǰ���ھ������ʧ��
	if ( !lpszKey || strlen(lpszKey) < 1 ) return FALSE;       //���������ϢΪfalse���߰�����Ϣ����Ϊ0������ʧ��
	if ( !SendComToSkype ( "BTN_PRESSED %s", lpszKey ) )
		return FALSE;
	if ( !SendComToSkype ( "BTN_RELEASED %s", lpszKey ) )
		return FALSE;
	return TRUE;
}	
//  %d ʮ�������� 
//  %c �ַ� 
//  %s �ַ��� 
//  %f, %g, %e ʮ���Ƹ����� 
//  %p ָ�� 
//  %o �˽��� 
//  %x ʮ������ 
//  %u �޷�������