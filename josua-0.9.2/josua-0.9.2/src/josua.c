/*  josua - Jack'Open SIP User Agent is a softphone for SIP.
    Copyright (C) 2002  Aymeric MOIZARD  - jack@atosc.org

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <osip/port.h>
#include "rcfile.h"
#include "josua.h"
#include <osip/sdp_negoc.h>
#include <osip/dialog.h>

#if defined(HAVE_SIGNAL_H) || defined(WIN32)
#include <signal.h>
#else /* HAVE_SYS_SIGNAL_H? */
#include <sys/signal.h>
#endif

int global_static_code=200; /* Ĭ�Ϸ�����ֵ���ɸ���default return code. Can be changed by user */

#ifdef WIN32
#include <stdio.h>
#include <stdlib.h>
#include <winsock.h>
#else
#include <unistd.h>

#include <sys/types.h>
#endif

#ifndef WIN32
#include <sys/time.h>
#include <libgen.h>
#endif

osip_t *myconfig;
ua_core_t *global_ua_core;
FILE *mylogfile;
FILE *configfile;
/* onsignal set kill_application to 1 when the application must be destroyed */
int  kill_application = 0;
extern char *register_callid_number;

void app_invite(char *tostring);
void app_cancel(); /* cancel the latest inviteȡ�����µ����� */
void app_options(char *tostring);
#ifdef ENABLE_DEBUG
void app_info();
#endif
void app_bye();

#ifdef WIN32
#include <process.h>
#include <io.h>
#include <conio.h>
#define onexit my_onexit
#define getpid _getpid
#define read _read
#define fgets(s, max, fp) gets(s)
#endif

void onexit();
void onsignal(int s);
void onalarm(int sig);

void normal_mode(char *callee);
#ifdef ENABLE_DEBUG
void test_mode_1(char *callee);
void test_mode_2(char *callee);
void test_mode_3(char *callee);
#endif

void josua_exit();

void
josua_want_exit(char *tmp)/*SIP�û�������绰������Ҫ�˳��Ự�ļ������*/
{
  if (tmp==NULL) return;
  if (kill_application!=0)
      josua_exit(kill_application);
  sclrspace(tmp);
  if (0==strncmp(tmp,"q",1)||0==strncmp(tmp,"e",1))
    { if (strlen(tmp)==1) { sfree(tmp); josua_exit(kill_application); } }
  if (0==strncmp(tmp,"quit",4)||0==strncmp(tmp,"exit",4))
    { if (strlen(tmp)==4) { sfree(tmp); josua_exit(kill_application); } }
}

void
josua_debug_level(char *tmp)
{
  if (0==strncmp(tmp,"0",1)) {   TRACE_ENABLE_LEVEL(0); }
  if (0==strncmp(tmp,"1",1)) {   TRACE_ENABLE_LEVEL(1); }
  if (0==strncmp(tmp,"2",1)) {   TRACE_ENABLE_LEVEL(2); }
  if (0==strncmp(tmp,"3",1)) {   TRACE_ENABLE_LEVEL(3); }
  if (0==strncmp(tmp,"4",1)) {   TRACE_ENABLE_LEVEL(4); }
  if (0==strncmp(tmp,"5",1)) {   TRACE_ENABLE_LEVEL(5); }
  if (0==strncmp(tmp,"d",1)) {
    TRACE_DISABLE_LEVEL(0);
    TRACE_DISABLE_LEVEL(1);
    TRACE_DISABLE_LEVEL(2);
    TRACE_DISABLE_LEVEL(3);
    TRACE_DISABLE_LEVEL(4);
    TRACE_DISABLE_LEVEL(5);
  }
}

void
josua_return_code(char *tmp)/*SIP�û���������*/
{
  if (0==strncmp(tmp,"s",1))
    {
#ifdef ENABLE_DEBUG
      int i;
#endif
      char *newcode;
      newcode = (char *)smalloc(11);
      fprintf(stdout,
	      "\t\tGive your status:\n\t status [200] [301]\t [302]\t [380]\t [486] ?");
      fflush(stdout);
      fgets(newcode,10,stdin);
      /* this crash when the user does not give a number����û���ѡ��״̬������ʾ���� */

#ifdef ENABLE_DEBUG
      i = atoi(newcode);
      if (100<=i&&i<700)
	global_static_code = i;
#else /* limit possibilities for end user���������û��Ŀ����ԣ�����ѡ��״̬���й� */
      if (0==strcmp(newcode, "200"))
	global_static_code = 200;
      else if (0==strcmp(newcode, "301"))
	global_static_code = 301;
      else if (0==strcmp(newcode, "302"))
	global_static_code = 302;
      else if (0==strcmp(newcode, "380"))
	global_static_code = 380;
      else if (0==strcmp(newcode, "486"))
	global_static_code = 486;
#endif
      else
	fprintf(stderr, "ERROR: Status code NOT changed!\n");
      sfree(newcode);
    }
}

#ifdef WIN32
char *
simple_readline(int descr, int forever)
{
  int i = 0;
  char *tmp = (char *)smalloc(201);

  if (forever) {
    fgets(tmp, 200, stdin);
  }
  else {
    while (kbhit()) {

      tmp[i] = (char)_getche();

      if (tmp[i] == '\r') {

	printf("\n");

	tmp[i] = '\0';

	i = 0;
      }
      else {

	if (tmp[i] == '\b') {
	  if (i > 0) {
	    printf(" ");
	    printf("\b");
	    i--;
	  }
	}
	else {
	  if (i < sizeof(tmp) - 2)
	    i++;
	}
      }
    }

    if (i == 0) {
      sfree(tmp);
      return(NULL);
    }
  }

  josua_want_exit(tmp);
  josua_debug_level(tmp);
  josua_return_code(tmp);
  return tmp;
}
#else
char *
simple_readline(int descr, int forever)
{
  int ret;
  struct timeval tv;
  fd_set fset;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&fset);
  FD_SET(descr, &fset);

  if (forever)
    {
      ret=0;
      while (kill_application==0&&ret==0)
	{ /* loop for ever, but keep an eye on kill_application's value����ѭ��������ʱ��ע��ֹ�����ֵ */
	  tv.tv_sec = 1;
	  tv.tv_usec = 0;
	  FD_ZERO(&fset);
	  FD_SET(descr, &fset);
	  ret = select(descr+1, &fset ,NULL ,
		       NULL, &tv );
	}
      if (kill_application!=0)
	{
	  josua_exit(kill_application);
	}
    }
  else
    ret = select(descr+1, &fset ,NULL ,
		 NULL, &tv );
  if (FD_ISSET(descr, &fset))
    {
      char *tmp;
      int i;
      tmp = (char *)smalloc(201);
      i = read(descr,tmp,200);
      if (i<0)
	{
	  sfree(tmp);
	  printf("Read error!...\n");
	  return NULL;
	}
      else
	{
	  tmp[i]='\0';
	  josua_want_exit(tmp);
	  josua_debug_level(tmp);
	  josua_return_code(tmp);
	}
      return tmp;
    }
  return NULL;
}
#endif

void
usage()/*ʹ�ò����б�*/
{
  printf("\n\
usage: josua [-f config] [-t \"sipurl\"] [-d level -l logfile]\n\
\n\
  -f  file             configuration file for your SIP phone.\n\
  -t  sipurl           The guy you want to talk to\n\
  -d  level            be verbose. 0 is no output. 6 is all output .\n\
  -l  logfile          specify the log file. or use - ~/josua.log - \n\n");
}

int/*��Ƶ��Ľ���*/
ua_sdp_accept_audio_codec(sdp_context_t *context,
			  char *port, char *number_of_port,
			  int audio_qty, char *payload)
{
  /* ִ�г����ṩ���SDP���ӣ����Ựֻ��Ҫһ������this may come from buggy implementation who                 */
  /* propose several sdp lines while they only want 1 connection */
  if (0!=audio_qty) return -1;

  if (0==strncmp(payload,"0",1)||0==strncmp(payload,"3",1)||
      0==strncmp(payload,"8",1)||0==strncmp(payload,"4",1))
    return 0;
  return -1;
}

int/*��Ƶ��Ľ���*/
ua_sdp_accept_video_codec(sdp_context_t *context,
			  char *port, char *number_of_port,
			  int video_qty, char *payload)
{
  /* this may come from buggy implementation who ͬ��                */
  /* propose several sdp lines while they only want 1 connection */
  if (0!=video_qty) return -1;
  /* ... */
  return -1;
}

int/*������Ľ���*/
ua_sdp_accept_other_codec(sdp_context_t *context,
			  char *type, char *port,
			  char *number_of_port, char *payload)
{
  /* ... */
  return -1;
}

char */*��ȡ��Ƶ�˿�*/
ua_sdp_get_audio_port(sdp_context_t *context, int pos_media)
{
  return sgetcopy("23010"); /* �ö˿ڲ��Ǿ�̬��this port should not be static ... */
  /* �����������Ҫ�����ʹ��also, this method should be called more than once... */
  /* ������ж���һ������Ƶ���ӣ����������ʧ��If there is more than one audio line, this may fail :( */
}

/* ���溯�����ڻ�ȡua_core����Ҫ����this is used to get the main instance of ua_core */
ua_core_t *ua_core_get()
{
  return global_ua_core;
}

int
main(int argc, char **argv)
{
  ua_core_t *ua_core;
  char *callee = NULL;
  char *logfile_name=NULL;
  char *configfile_name=NULL;
  int trace_level=-1;
  int arg_num;
  int i;
#ifdef ENABLE_DEBUG
  int mode = 0;        /* ģʽֵӦ�ý���0��2֮��should be between 0 and 2 */
#endif
#ifdef WIN32
  int err;
  WSADATA wsdata;

  err = WSAStartup(MAKEWORD(2, 0), &wsdata);
#else
  /*  atexit(&onexit); */
#endif

  configfile = NULL;
  mylogfile = NULL;
#ifndef WIN32
  signal(SIGINT, &onsignal);
#endif
  /*********************************/
  /*  Parse arguments  ��������            */

  arg_num = 1;
  while (arg_num<argc)
    {
      int old_arg_num = arg_num;
      if (strlen(argv[arg_num])!=2)
	{
	  usage();
	  exit(0);
	}
      if (strncmp("-f",argv[arg_num],2)==0)
	{
	  /* the next arg is the config file��һ��arg�������ļ� */
	  arg_num++;
	  configfile_name = argv[arg_num];
	}
      if (strncmp("-t",argv[arg_num],2)==0)
	{
	  /* the next arg is the address in the to field��һ��arg�ǵ�ַ */
	  arg_num++;
	  callee =  argv[arg_num];
	}
#ifdef ENABLE_DEBUG
      if (strncmp("-m",argv[arg_num],2)==0)
	{
	  /* the next arg is the mode to start...��һ��arg������ģʽ */
	  arg_num++;
	  mode = atoi(argv[arg_num]);
	}
#endif
      if (strncmp("-d",argv[arg_num],2)==0)
	{
	  /* the next arg is the trace level... ��һ��arg��·���ȼ�*/
	  arg_num++;
	  trace_level = atoi(argv[arg_num]);
	}
      if (strncmp("-l",argv[arg_num],2)==0)
	{
	  /* the next arg is the log file...��һ��arg����־�ļ� */
	  arg_num++;
	  logfile_name = argv[arg_num];
	}
      if (old_arg_num==arg_num)
	{
	  fprintf(stderr, "ERROR: bad arguments\n");
	  usage();
	  exit(0);
	}	  
      arg_num++;
    }

  /*********************************/
  /*   Load Config File ���������ļ�           */

  if (configfile_name==NULL)
    i = josua_config_load("/home/jack/.josua/config");
  else
    i = josua_config_load(configfile_name);
  if (i!=0)
      {
	perror("ERROR: Could not load config file!");
	usage();
	exit(1);
      }

  /*********************************/
  /* INIT Log File and Log LEVEL��־�ļ�   */

  if (trace_level>=0)
    {
      if (logfile_name==NULL)
	{
	  if (josua_config_get_element("logfile")!=NULL)
	    mylogfile = fopen(josua_config_get_element("logfile"), "w+");
	}
      else
	mylogfile = fopen(logfile_name, "w+");

      fprintf(stderr, "INFO: init the logfile\n");
      TRACE_INITIALIZE(trace_level,mylogfile);
    }
  

  /*********************************/
  /*  SDP init   �Ự����Э��                  */

  i = sdp_config_init();
  if (i!=0) {
    fprintf(stderr,"ERROR: Could not initialize the SDP negociator\n");
    exit(1);
  }
  sdp_config_set_o_username(sgetcopy("userX"));
  sdp_config_set_o_session_id(sgetcopy("20000001"));
  sdp_config_set_o_session_version(sgetcopy("20000001"));
  sdp_config_set_o_nettype(sgetcopy("IN"));
  sdp_config_set_o_addrtype(sgetcopy("IP4"));
  sdp_config_set_o_addr(sgetcopy("192.168.1.114"));
  
  sdp_config_set_c_nettype(sgetcopy("IN"));
  sdp_config_set_c_addrtype(sgetcopy("IP4"));
  sdp_config_set_c_addr(sgetcopy("192.168.1.114"));
  
  /* ���б��빲��"C="�У�ý����ʾΪͬһ��"m"��ALL CODEC MUST SHARE THE SAME "C=" line and proto as the media 
     will appear on the same "m" line... */
  sdp_config_add_support_for_audio_codec(sgetcopy("0"),
					 NULL,
					 sgetcopy("RTP/AVP"),
					 NULL, NULL, NULL,
					 NULL,NULL,
					 sgetcopy("0 PCMU/8000"));
  sdp_config_add_support_for_audio_codec(sgetcopy("3"),
					 NULL,
					 sgetcopy("RTP/AVP"),
					 NULL, NULL, NULL,
					 NULL,NULL,
					 sgetcopy("3 GSM/8000"));
  sdp_config_add_support_for_audio_codec(sgetcopy("7"),
					 NULL,
					 sgetcopy("RTP/AVP"),
					 NULL, NULL, NULL,
					 NULL,NULL,
					 sgetcopy("7 LPC/8000"));
  sdp_config_add_support_for_audio_codec(sgetcopy("8"),
					 NULL,
					 sgetcopy("RTP/AVP"),
					 NULL, NULL, NULL,
					 NULL,NULL,
					 sgetcopy("8 PCMA/8000"));
  
  sdp_config_set_fcn_accept_audio_codec(&ua_sdp_accept_audio_codec);
  sdp_config_set_fcn_accept_video_codec(&ua_sdp_accept_video_codec);
  sdp_config_set_fcn_accept_other_codec(&ua_sdp_accept_other_codec);
  sdp_config_set_fcn_get_audio_port(&ua_sdp_get_audio_port);
  
  
  osip_init(&myconfig);
  
#ifndef OSIP_MT
  if (0!=ua_core_global_init(myconfig))
    {
      fprintf(stderr, "ERROR: Can't initialize osip!\n");
      exit(0);
    }
  ua_core_init(&ua_core, atoi(josua_config_get_element("localport")), 0, myconfig);

  ua_global_core = ua_core;
  {
    clock_t clock1,clock2;
    clock1 = time(NULL);
    while (1)
      {
	/*	  transaction_t *transaction; */
	int i;
	/* do it only each 500ms���Ϊ500ms */
	/* this may not be possible on non-real time OS ����Ϊʵʱ��OS*/
	clock2 = time(NULL);
	if (clock2-clock1>2) /* more than one sec����1�� */
	  {
	    OSIP_TRACE(osip_trace(__FILE__,__LINE__,OSIP_INFO4,NULL,"TODO: start timers management:)\n"));
	    clock1 = clock2;
	    /*
	      timers_execute(myconfig->uas_transactions);
	      timers_execute(myconfig->uac_transactions);
	    */
	  }

	osip_ict_execute(myconfig);
	osip_ist_execute(myconfig);
	osip_nict_execute(myconfig);
	osip_nist_execute(myconfig);

	/* start a UDP layer on port �ڶ˿��Ͽ���һ��UDP�� */
	i=ua_core_udp_tl_execute(ua_core,0,0,1);
	if (i<0) {
	  fprintf(stderr,"ERROR: Could not start udp layer\n");
	  exit(0);
	}

	/* TODO timer managements... 
	   transaction = (transaction_t *)fifo_tryget(myconfig->uas_timerff);
	   while (transaction!=NULL)
	   {
	   list_add(myconfig->uas_transactions,transaction,-1);
	   transaction = (transaction_t *)fifo_tryget(myconfig->uas_timerff);
	   }

	   transaction = (transaction_t *)fifo_tryget(myconfig->uac_timerff);
	   while (transaction!=NULL)
	   {
	   list_add(myconfig->uac_transactions,transaction,-1);
	   transaction = (transaction_t *)fifo_tryget(myconfig->uac_timerff);
	   }
	*/
      } 
  }
#endif
#ifdef OSIP_MT

  /*********************************/
  /*  SDP init  �Ự����Э��                   */

  if (0!=ua_core_global_init(myconfig))
    {
      fprintf(stderr, "ERROR: Can't configure the transaction layer!\n");
      exit(0);
    }
  ua_core_init(&ua_core, atoi(josua_config_get_element("localport"))
	       , 0, myconfig);

  global_ua_core = ua_core;
  /* start a UDP layer on port�ڶ˿ڿ���һ��UDP��  */
  ua_core_udp_tl_start(ua_core);

  ua_core_timers_init(ua_core);

  fprintf(stdout, "\n\ttype ? for keystrokes:\n\n");

#ifdef ENABLE_DEBUG
  if (mode==3) /* high load test �߸��ز���*/
    {
      test_mode_3(callee);
    }
  if (mode==2) /* high load test�߸��ز��� */
    {
      test_mode_2(callee);
    }
  if (mode==1) /* slower test �͸��ز���*/
    {
      test_mode_1(callee);
    }

  if (mode==0) /* Command mode����ģʽ */
#endif
      normal_mode(callee);

#endif    
  exit(0);
  return 1; /* ok��� */

}

void
uaapp_failure(transaction_t *tr, char *error)/*�շ������Ӧ����*/
{
  printf("error: %s", error);
}

void   uaapp_snd_invite(transaction_t *tr, sip_t *invite)
{

}

void   uaapp_snd_ack(transaction_t *tr, sip_t *ack)
{

}

void   uaapp_snd_bye(transaction_t *tr, sip_t *request)
{

}

void   uaapp_snd_register(transaction_t *tr, sip_t *request)
{

}

void   uaapp_snd_options(transaction_t *tr, sip_t *request)
{

}

void   uaapp_snd_info(transaction_t *tr, sip_t *request)
{

}

void   uaapp_snd_cancel(transaction_t *tr, sip_t *request)
{

}

void   uaapp_snd_subscribe(transaction_t *tr, sip_t *request)
{

}

void   uaapp_snd_notify(transaction_t *tr, sip_t *request)
{

}

void   uaapp_snd_unknown(transaction_t *tr, sip_t *request)
{

}

void uaapp_snd1xx_invite(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd2xx_invite(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd3xx_invite(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd4xx_invite(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd5xx_invite(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd6xx_invite(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd1xx_register(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd2xx_register(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd3xx_register(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd4xx_register(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd5xx_register(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd6xx_register(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd1xx_bye(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd2xx_bye(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd3xx_bye(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd4xx_bye(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd5xx_bye(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd6xx_bye(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd1xx_options(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd2xx_options(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd3xx_options(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd4xx_options(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd5xx_options(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd6xx_options(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd1xx_info(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd2xx_info(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd3xx_info(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd4xx_info(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd5xx_info(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd6xx_info(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd1xx_cancel(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd2xx_cancel(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd3xx_cancel(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd4xx_cancel(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd5xx_cancel(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd6xx_cancel(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd1xx_notify(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd2xx_notify(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd3xx_notify(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd4xx_notify(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd5xx_notify(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd6xx_notify(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd1xx_subscribe(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd2xx_subscribe(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd3xx_subscribe(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd4xx_subscribe(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd5xx_subscribe(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd6xx_subscribe(transaction_t *tr, sip_t *response)
{

}


void uaapp_snd1xx_unknown(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd2xx_unknown(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd3xx_unknown(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd4xx_unknown(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd5xx_unknown(transaction_t *tr, sip_t *response)
{

}

void uaapp_snd6xx_unknown(transaction_t *tr, sip_t *response)
{

}

void   uaapp_rcv_invite(transaction_t *tr, sip_t *invite)
{
  generating_answer_to_invite(tr,100);
  generating_answer_to_invite(tr,180);
  generating_answer_to_invite(tr,global_static_code);
}

void   uaapp_rcv_ack(transaction_t *tr, sip_t *ack)
{

}

void   uaapp_rcv_bye(transaction_t *tr, sip_t *request)
{
  generating_answer_to_bye(tr,200); /* ��������ڶԻ�����ı����code will be changed if no dialog exist */
}

void   uaapp_rcv_register(transaction_t *tr, sip_t *request)
{
  generating_default_response(tr,500);
}

void   uaapp_rcv_options(transaction_t *tr, sip_t *request)
{
  generating_answer_to_options(tr,global_static_code);
}

void   uaapp_rcv_info(transaction_t *tr, sip_t *request)
{
  generating_default_response(tr,global_static_code);
}

void   uaapp_rcv_cancel(transaction_t *tr, sip_t *request)
{
  int pos = 0;
  ua_core_t *ua_core;
  transaction_t *ist_tr;
  ua_core = ua_core_get();
  /* find the corresponding INVITE transaction�ҵ���Ӧ����������
     (I do not support CANCEL for other requests��֧��ȡ������������ */
  osip_ist_lock(ua_core->config);
  while (!list_eol(ua_core->config->ist_transactions, pos))
    {
      ist_tr = list_get(ua_core->config->ist_transactions, pos);
      /* request and CANCEL request must match �����ȡ����������Ӧ*/
      if ((ist_tr->state==IST_PRE_PROCEEDING)||
	  (ist_tr->state==IST_PROCEEDING))
	{
	  if ((0==call_id_match(ist_tr->callid,
				request->call_id))
	      && (0==callleg_match(ist_tr->to,
				   ist_tr->from,
				   request->to,
				   request->from))
	      && (0==strcmp(ist_tr->cseq->number,request->cseq->number)))
	    {
	      /* INVITE found! �������룬Ѹ�ٻ�Ӧ487*/
	      /* answer 487 quickly  :-) */
	      osip_ist_unlock(ua_core->config);
	      generating_answer_to_invite(ist_tr,487);
	      generating_default_response(tr,200);
	      return;
	    }
	}
      pos++;
    }
  osip_ist_unlock(ua_core->config);
  generating_default_response(tr,481);
}

void   uaapp_rcv_subscribe(transaction_t *tr, sip_t *request)
{
  generating_default_response(tr,500);
}

void   uaapp_rcv_notify(transaction_t *tr, sip_t *request)
{
  generating_default_response(tr,500);
}

void   uaapp_rcv_unknown(transaction_t *tr, sip_t *request)
{
  generating_default_response(tr,500);
}

void uaapp_rcv1xx_invite(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv2xx_invite(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv3xx_invite(transaction_t *tr, sip_t *response)/*�����ض�������*/
{
  int i;
  char *to;
  char *subject;
  header_t *head;

  contact_t *contact;
  to = NULL;

  contact = list_get(response->contacts, 0);
  if (contact==NULL)
    goto error;
  if (MSG_TEST_CODE(response, 301)) /* moved permanently */
    {
      printf("301 Moved permanently, we should update the Add Book!\n");
    }
  else if (MSG_TEST_CODE(response, 380)) /* alternative serviceѡ���� */
    { /* do not continue... just announce the error���������㣬������� */
      /* how can we relate this to some context??? */
      /* uaapp_use_alternate_service(tr->orig_request, contact); */
      printf("380 Use alternate services.\n");
      return;
    }

  /* create a new invite transaction ����һ���µ������¼�*/
  i = contact_2char(contact, &to);
  if (i!=0) /* ??? */
    goto error;

  /* TODO: */
  /*   1: ���Ǳ������������we must remove extra parameters in the to field! (and rquri!) */
  /*   2: �����������һ�������Ǳ��뽨��һϵ�е���ϵwe must prepare a list of contact to try if more than one is given */

  i = msg_header_getbyname(tr->orig_request, "subject", 0, &head);
  printf("before getting subject\n");
  if (head==NULL) /* ??? */
    subject = sgetcopy("New Call");
  else
    {
      if (head->hvalue==NULL)
	subject = sgetcopy("New Call");
      else
	subject = sgetcopy(head->hvalue);
    }
  printf("after getting subject\n");
  sip_invite(to, subject);
  sfree(subject);
  sfree(to);
  return;

 error:
  sfree(to);
  uaapp_failure(tr, "error in incoming 3xx message: No contact header!");
  return;
}

void uaapp_rcv4xx_invite(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv5xx_invite(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv6xx_invite(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv1xx_register(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv2xx_register(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv3xx_register(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv4xx_register(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv5xx_register(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv6xx_register(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv1xx_bye(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv2xx_bye(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv3xx_bye(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv4xx_bye(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv5xx_bye(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv6xx_bye(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv1xx_options(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv2xx_options(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv3xx_options(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv4xx_options(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv5xx_options(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv6xx_options(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv1xx_info(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv2xx_info(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv3xx_info(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv4xx_info(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv5xx_info(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv6xx_info(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv1xx_cancel(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv2xx_cancel(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv3xx_cancel(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv4xx_cancel(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv5xx_cancel(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv6xx_cancel(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv1xx_notify(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv2xx_notify(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv3xx_notify(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv4xx_notify(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv5xx_notify(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv6xx_notify(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv1xx_subscribe(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv2xx_subscribe(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv3xx_subscribe(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv4xx_subscribe(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv5xx_subscribe(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv6xx_subscribe(transaction_t *tr, sip_t *response)
{

}


void uaapp_rcv1xx_unknown(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv2xx_unknown(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv3xx_unknown(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv4xx_unknown(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv5xx_unknown(transaction_t *tr, sip_t *response)
{

}

void uaapp_rcv6xx_unknown(transaction_t *tr, sip_t *response)
{

}

void
app_invite(char *callee)
{
  int transactionid;
  transactionid = sip_invite(callee, "Hello.");
}

void
app_cancel()
{ /* cancel the latest invite �������������*/
#ifdef OSIP_MT
  transaction_mt_t *transaction_mt;
#endif
  sip_t         *cancel;
  transaction_t *ict_transaction;
  transaction_t *transaction;
  ua_core_t *ua_core;
  int i;
  int pos=0;
  ua_core = ua_core_get();

  /*����һ���ٽ��棨����ת���¼��� enter a critical section (the transaction can be removed) */
  osip_ict_lock(ua_core->config);
  /* find the first INVITE that can be CANCELED!�ҵ���һ�����Ա�ȡ�������� */
  do {
    ict_transaction = list_get(ua_core->config->ict_transactions, pos);
    pos++;
  } while (ict_transaction!=NULL
	   && ict_transaction->state!=ICT_CALLING
	   && ict_transaction->state!=ICT_PROCEEDING);
  if (ict_transaction==NULL)
    {
      printf("No INVITE transaction to cancel\n");
      osip_ict_unlock(ua_core->config);
      return;
    }
  if (ict_transaction->state==ICT_CALLING)
    {
      /* ֻ�г�������100�κ��ȡ��CANCEL are only sent when a 100 trying has been received */
      osip_ict_unlock(ua_core->config);
      return;
    }
  i = generating_cancel(&cancel, ict_transaction->orig_request);
  if (i!=0)
    {
      printf("ERROR: could not CANCEL the INVITE\n");
      osip_ict_unlock(ua_core->config);
      return ;
    }
  osip_ict_unlock(ua_core->config);
  transaction_init(&transaction,
		   NICT,
		   ua_core->config,
		   cancel);
#ifdef OSIP_MT
  transaction_mt_init(&transaction_mt);
  transaction_mt_set_transaction(transaction_mt,transaction);
  transaction_mt_start_transaction(transaction_mt);
#endif

  osip_ul_sendmsg(transaction,cancel);  
  
  return; /* transaction->transactionid; */

}

void
app_bye()
{
  /* select the first dialog and send a BYEѡ���һ���Ự������"BYE" */
  ua_core_t *ua_core;
  dialog_t *dialog;
  ua_core = ua_core_get();
  ua_core_lock_dialog_access(ua_core);
  dialog = list_get(ua_core->dialogs, 0);
  /* �������Ժ����the unlock should be done later!! */
  ua_core_unlock_dialog_access(ua_core);
  if (dialog==NULL) return;
  if (dialog->state==DIALOG_EARLY)
    { /* ����Ự���û�����ͻ��˵���ݱ����룬���ǾͿ��������ڻỰ����һ��"BYE"if this dialog has been initiated as a UAC,
	 we are allowed to send a BYE to early dialog */
      if (dialog->type==CALLER)
	{
	  sip_bye(dialog);
	  /* sip_cancel(); ���ǷǷ��ģ���Ϊ���ǲ�û������ȡ����Ӧ���¼�this is disabled because it does not cancel
	     the correct transaction by now */
	  return;
	} /* �ȴ���ɲ���������else wait for completion and then bye the call... */
    }
  sip_bye(dialog);
}

void
app_options(char *callee)
{
  /* select the first dialog and send a OPTIONSѡ���һ���Ự������"OPTIONS" */
  ua_core_t *ua_core;
  dialog_t *dialog;
  ua_core = ua_core_get();
  ua_core_lock_dialog_access(ua_core);
  dialog = list_get(ua_core->dialogs, 0);
  ua_core_unlock_dialog_access(ua_core);
  if (dialog==NULL)
    sip_options(NULL, callee);
  else
    sip_options(dialog, NULL); /* ����usable even of dialog is state==DIALOG_EARLY */
}

#ifdef ENABLE_DEBUG
void
app_info()
{
  /* select the first dialog and send an INFOѡ���һ���Ự������"INFO"  */
  ua_core_t *ua_core;
  dialog_t *dialog;
  ua_core = ua_core_get();
  ua_core_lock_dialog_access(ua_core);
  dialog = list_get(ua_core->dialogs, 0);
  ua_core_unlock_dialog_access(ua_core);
  if (dialog==NULL) return;
  sip_info(dialog);
}
#endif

/* for use of int atexit(void (*func)(void) */
void
josua_exit(int i){
  int pos=0;
  ua_core_t *ua_core;
  kill_application = 1;
  printf("Shutting down all the stack\n");
  if (register_callid_number!=NULL) sfree(register_callid_number);
  if (configfile!=NULL)
    fclose(configfile);
  ua_core = ua_core_get();
  if (ua_core==NULL) exit(i);
  printf("Removing listener thread : ");
  ua_core_udp_tl_stop(ua_core);
  i = sthread_join(ua_core->udp_tl->thread);
  if (i!=0)
    printf("I can't do that? (unknown error)\n");
  else
    printf("done\n");

  /* Must have a way to stop the timers thread! */
  /* this is a quick hack����ֹͣ��ʱ�� */
  ua_core->timers_delete = 1; /* delete flagɾ��flag */
  if (ua_core->timers!=NULL)
    {
      i = sthread_join(ua_core->timers);
      if (i!=0) printf("ERROR: can't stop timer thread\n");
#ifndef __VXWORKS_OS__
      sfree(ua_core->timers);
#endif
      ua_core->timers = NULL;
    }

  if (ua_core->config==NULL) exit(i);
  printf("Removing all ICT threads\n");
  pos=0;
  while (!list_eol(ua_core->config->ict_transactions, pos))
    {
      transaction_t *transaction = list_get(ua_core->config->ict_transactions,pos);
      transaction_mt_t *transaction_mt = transaction->your_instance;
#ifdef HAVE_PTHREAD_H
      printf("shutting down thread (%li) : ", *transaction_mt->thread);
#else
      printf("shutting down thread : ");
#endif
      if (transaction_mt->thread!=NULL)
	{
	  sipevent_t *sipevent;
	  sipevent = osip_new_event(KILL_TRANSACTION,transaction->transactionid);
	  fifo_add(transaction->transactionff,sipevent);
	  
	  /* fifo_add(ua_core->threads_to_join, transaction_mt); */
	  i = sthread_join(transaction_mt->thread);
	  
	  if (i!=0)
	    {
	      printf("done (thread previously stopped)\n");
	    }
	  else
	    {
	      transaction_free(transaction);
	      sfree(transaction);
	      sfree(transaction_mt->thread);
	      sfree(transaction_mt);
	      printf("done\n");
	    }
	}
	else
	  {
	    pos++;
	    printf("I can't do that? (unknown error)\n");
	  }
    }

  printf("Removing all IST threads\n");
  pos=0;
  while (!list_eol(ua_core->config->ist_transactions, pos))
    {
      transaction_t *transaction = list_get(ua_core->config->ist_transactions,pos);
      transaction_mt_t *transaction_mt = transaction->your_instance;
#ifdef HAVE_PTHREAD_H
      printf("shutting down thread (%li) : ", *transaction_mt->thread);
#else
      printf("shutting down thread : ");
#endif
#ifndef __VXWORKS_OS__
	if (transaction_mt->thread!=NULL)
#else
        if (transaction_mt->thread!=ERROR)
#endif
	  {
	    sipevent_t *sipevent;
	    sipevent = osip_new_event(KILL_TRANSACTION,transaction->transactionid);
	    fifo_add(transaction->transactionff,sipevent);
	    
	    /* fifo_add(ua_core->threads_to_join, transaction_mt); */
	    i = sthread_join(transaction_mt->thread);
	    if (i!=0)
	      {
		printf("done (thread previously stopped)\n");
	      }
	    else
	      {
		transaction_free(transaction);
		sfree(transaction);
		sfree(transaction_mt->thread);
		sfree(transaction_mt);
		printf("done\n");
	      }
	  }
	else
	  {
	    pos++;
	    printf("I can't do that? (unknown error)\n");
	  }
    }

  printf("Removing all NICT threads\n");
  pos=0;
  while (!list_eol(ua_core->config->nict_transactions, pos))
    {
      transaction_t *transaction = list_get(ua_core->config->nict_transactions,pos);
      transaction_mt_t *transaction_mt = transaction->your_instance;
#ifdef HAVE_PTHREAD_H
      printf("shutting down thread (%li) : ", *transaction_mt->thread);
#else
      printf("shutting down thread : ");
#endif
#ifndef __VXWORKS_OS__
	if (transaction_mt->thread!=NULL)
#else
        if (transaction_mt->thread!=ERROR)
#endif
	  {
	    sipevent_t *sipevent;
	    sipevent = osip_new_event(KILL_TRANSACTION,transaction->transactionid);
	    fifo_add(transaction->transactionff,sipevent);
	    
	    /* fifo_add(ua_core->threads_to_join, transaction_mt); */
	    i = sthread_join(transaction_mt->thread);
	    if (i!=0)
	      {
		printf("done (thread previously stopped)\n");
	      }
	    else
	      {
		transaction_free(transaction);
		sfree(transaction);
		sfree(transaction_mt->thread);
		sfree(transaction_mt);
		printf("done\n");
	      }
	  }
	else
	  {
	    pos++;
	    printf("I can't do that? (unknown error)\n");
	  }
    }

  printf("Removing all NIST threads\n");
  pos=0;
  while (!list_eol(ua_core->config->nist_transactions, pos))
    {
      transaction_t *transaction = list_get(ua_core->config->nist_transactions,pos);
      transaction_mt_t *transaction_mt = transaction->your_instance;
#ifdef HAVE_PTHREAD_H
      printf("shutting down thread (%li) : ", *transaction_mt->thread);
#else
      printf("shutting down thread : ");
#endif
#ifndef __VXWORKS_OS__
	if (transaction_mt->thread!=NULL)
#else
        if (transaction_mt->thread!=ERROR)
#endif
	  {
	    sipevent_t *sipevent;
	    sipevent = osip_new_event(KILL_TRANSACTION,transaction->transactionid);
	    fifo_add(transaction->transactionff,sipevent);
	    /* fifo_add(ua_core->threads_to_join, transaction_mt); */
	    i = sthread_join(transaction_mt->thread);
	    if (i!=0)
	      {
		printf("done (thread previously stopped)\n");
	      }
	    else
	      {
		transaction_free(transaction);
		sfree(transaction);
		sfree(transaction_mt->thread);
		sfree(transaction_mt);
		printf("done\n");
	      }
	  }
	else
	  {
	    pos++;
	    printf("I can't do that? (unknown error)\n");
	  }
    }


  /* a memory leak exists in pthread, I don't know how to remove it. */

  sdp_config_free();
  
  ua_core_free(ua_core);
  sfree(ua_core);
  printf("ole3\n");
  
  josua_config_unload();
  osip_global_free();

  OSIP_TRACE(osip_trace(__FILE__,__LINE__,OSIP_ERROR,NULL,"program end normally :)\n"));

  if (mylogfile!=NULL)
    fclose(mylogfile); /* �رգ�stdout��do we close(stdout); ?? */
  exit(i);
}

#if defined HAVE_SIGNAL_H || defined HAVE_SYS_SIGNAL_H 
/* for use of void (*signal(int sig, void (*func)(int)))(int);  */
void
onsignal(int s)
{
  /*  printf("close josua with q (%i) :(\n", s); */
  kill_application = s;
}
#endif

void
normal_mode(char *callee)
{ 
  char *tmp;
  int i=1;
  
  while (i!=0)
    {
      fprintf(stdout, "(josua) ");
      fflush(stdout);
      tmp = simple_readline(0,1);

      if (0==strncmp(tmp,"i",1))
	app_invite(callee);
      else if (0==strncmp(tmp,"r",1))
	{
	  sip_register();
	} 
      else if (0==strncmp(tmp,"b",1))
	{
	  int j;
	  sfree(tmp);
	  tmp = (char *)smalloc(100);
	  fprintf(stdout,"Number of BYE transaction to send. (Give an integer or crash!)\n");
	  fgets(tmp, 100, stdin);
	  j = atoi(tmp);
	  while (j!=0)
	    {
	      app_bye();
	      j--;
	    }
	}
      else if (0==strncmp(tmp,"o",1))
	app_options(callee);
      else if (0==strncmp(tmp,"c",1))
	app_cancel();
#ifdef ENABLE_DEBUG
      else if (0==strncmp(tmp,"n",1))
	app_info();
      else if (0==strncmp(tmp,"t",1))
	{
	  int j;
	  sfree(tmp);
	  tmp = (char *)smalloc(100);
	  fprintf(stdout,"Number of INVITE transaction to send. (Give an integer or crash!)\n");
	  fgets(tmp, 100, stdin);
	  j = atoi(tmp);
	  while (j!=0)
	    {
	      app_invite(callee);
	      j--;
	    }
	}
      else
	{
	  printf("\
    keystrokes:\n\
r:\t  REGISTER register your location.\n\
i:\t  INVITE   make a call\n\
c:\t  CANCEL   cancel the first pending call\n\
o:\t  OPTIONS  query for capabilities of remote UA\n\
b:\t  BYE      Hook off your active dialog\n\
s:\t  Update your status [200] [301] [302] [380] [486] (current:%i)\n\
q:\t  quit\n",
		 global_static_code);
	}
#endif
      if (tmp!=NULL) sfree(tmp);
    }
}

#ifdef ENABLE_DEBUG
void
test_mode_1(char *callee)
{
      int active = 1;
      char *tmp;
      printf("\n This should only be used for testing. Use with care!\n");
      printf(" Test mode: LOW TEST MODE!\n\n");
      while (1)
	{
	  tmp = simple_readline(0,0);
	  if (tmp!=NULL)
	    {
	      if (strncmp(tmp,"s",1)==0)
		active = 0;
	      if (strncmp(tmp,"r",1)==0)
		active = 1;
	      if (strncmp(tmp,"?",1)==0)
		{
		  printf("\
0 1 2 3 4 5:             enable trace levels   d: disable trace\n\
c:                       new static return code wanted\n\
s:                       stop test\n\
r:                       restart test\n\
q:                       quit\n");
		}
	      sfree(tmp);
	    }
	  susleep(500000); /* ok for 1s */
	  if (active==1)
	      app_invite(callee);
	}
}

void
test_mode_2(char *callee)
{
  int active = 1;
  int i=10;
  printf("\n This should only be used for testing. Use with care!\n");
  printf(" Test mode: HIGH LOAD TEST!\n\n");
  while (i!=0)
    {
      int j=50;
      char *tmp;
      
      tmp = simple_readline(0,0);
      if (tmp!=NULL)
	{
	  if (strncmp(tmp,"s",1)==0)
	    active = 0;
	  if (strncmp(tmp,"r",1)==0)
	    active = 1;
	  if (strncmp(tmp,"?",1)==0)
	    {
	      printf("\
0 1 2 3 4 5:             enable trace levels   d: disable trace\n\
c:                       new static return code wanted\n\
c:                       choose a new static return code.\n\
s:                       stop test\n\
r:                       restart test\n\
q:                       quit\n");
	    }
	  sfree(tmp);
	}
      if (active==1)
	{
	  int pending_transactions = list_size(myconfig->ict_transactions)
	    + list_size(myconfig->ist_transactions)
	    + list_size(myconfig->nict_transactions)
	    + list_size(myconfig->nist_transactions);
	      /*�����¼����� limit the number of transactions */
	  while (j!=0)
	    {
	      if (pending_transactions<600)
		app_invite(callee);
	      else
		susleep(200000); /*�������� the application is overloeaded... */
		  j--;
	    }
	  j=50;
	  while (j!=0)
	    {
	      app_bye();
	      j--;
	    }
	  /*  i--; */ /* loop for ever... */
	}
      /* susleep(1000000); */
    }
}


void
test_mode_3(char *callee)
{
  int active = 1;
  int i=10;
  printf("\n This should only be used for testing. Use with care!\n");
  printf(" Test mode: HIGH LOAD TEST!\n\n");
  while (i!=0)
    {
      int j=50;
      char *tmp;
      
      tmp = simple_readline(0,0);
      if (tmp!=NULL)
	{
	  if (strncmp(tmp,"s",1)==0)
	    active = 0;
	  if (strncmp(tmp,"r",1)==0)
	    active = 1;
	  if (strncmp(tmp,"?",1)==0)
	    {
	      printf("\
0 1 2 3 4 5:             enable trace levels   d: disable trace\n\
c:                       new static return code wanted\n\
c:                       choose a new static return code.\n\
s:                       stop test\n\
r:                       restart test\n\
q:                       quit\n");
	    }
	  sfree(tmp);
	}
      j=20;
      while (j!=0)
	{
	  app_bye();
	  j--;
	}
      susleep(5000000);
    }
}

#endif
