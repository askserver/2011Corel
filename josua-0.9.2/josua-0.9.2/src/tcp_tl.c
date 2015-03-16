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


#ifdef ENABLE_MPATROL
#include <mpatrol.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#ifndef __VXWORKS_OS__
#ifdef WIN32
#include <winsock.h>
#define close(s) closesocket(s)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/time.h>
#endif
#endif

#ifdef __VXWORKS_OS__
#include "vxWorks.h" 
#include "sockLib.h" 
#include "inetLib.h" 
#endif

#include "josua.h"
#include <osip/port.h>
#include <osip/const.h>

/*TCP����������*/
int
tcp_transport_layer_init(tcp_transport_layer_t **tcp_tl,/*TCP������ʼ��*/
			 int in_port, int out_port)/*��������˿�*/
{
  *tcp_tl = (tcp_transport_layer_t*)smalloc(sizeof(tcp_transport_layer_t));
  if (*tcp_tl==NULL) return -1;
  return 0;
#ifdef OSIP_TCP
  struct sockaddr_in  raddr;/*�ӿڵ�ַ*/
  int option=1;

  *tcp_tl = (tcp_transport_layer_t*)smalloc(sizeof(tcp_transport_layer_t));
  if (*tcp_tl==NULL) return -1;/*TCP����㲻���ڣ��򷵻�-1*/
  
  if (pipe((*tcp_tl)->control_fds)!=0)/*����TCP�����*/
  {
    perror("Error creating pipe");
    exit(1);
  }
  /* �������ļ���������д���ջ����the file descriptor where to write something to control the stack */
  
  (*tcp_tl)->in_port = in_port;

  /* ���ڻ���ʹ��not used by now */
  (*tcp_tl)->out_port = out_port;

  (*tcp_tl)->out_socket = (int)socket(PF_INET, SOCK_DGRAM, IPPROTO_TCP);
  if ((*tcp_tl)->out_socket==-1)/*����˿�Ϊ-1*/
    {
      OSIP_TRACE(osip_trace(__FILE__,__LINE__,OSIP_ERROR,NULL,
		  "ERROR: TCP sender cannot create descriptor %i!\n",
		  (*tcp_tl)->in_port));
      return -1;
    }

  (*tcp_tl)->in_socket = (int)socket(PF_INET, SOCK_DGRAM, IPPROTO_TCP);

  if ((*tcp_tl)->in_socket==-1) {/*ͬ������˿�Ϊ-1*/
    OSIP_TRACE(osip_trace(__FILE__,__LINE__,OSIP_ERROR,NULL,
		"ERROR: TCP listener cannot create descriptor %i!\n",
		(*tcp_tl)->in_port));
    exit(0);
  }
  if (out_port==in_port)/*�������ͬһ�˿�*/
    {
      (*tcp_tl)->in_socket = (*tcp_tl)->out_socket;
    }
  else
    {
      
      raddr.sin_addr.s_addr = htons(INADDR_ANY);
      raddr.sin_port = htons((short)(*tcp_tl)->in_port);
      raddr.sin_family = AF_INET;
      
      if (bind((*tcp_tl)->in_socket,/*������˿�*/
	       (struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
	TRACE(osip_trace(__FILE__,__LINE__,OSIP_ERROR,NULL,"ERROR: TCP listener bind failed %i!\n", (*tcp_tl)->in_port));
	exit(0);
      }
    }
  
  if (0==setsockopt((*tcp_tl)->in_socket,
		    SOL_SOCKET , SO_REUSEADDR, (void*)&option, sizeof(option)))
    {
     OSIP_TRACE(osip_trace(__FILE__,__LINE__,OSIP_ERROR,NULL,"WARNING: TCP listener SO_REUSE_ADDR failed %i!\n", (*tcp_tl)->in_port));
    }
#endif
return 0;
}

void
tcp_transport_layer_free(tcp_transport_layer_t *tcp_tl)/*�ͷ�TCP�����*/
{
#ifdef OSIP_TCP
  if (tcp_tl->in_socket!=-1)/*�ر���������˿ڲ��رտ���*/
    close(tcp_tl->in_socket);
  if (tcp_tl->out_socket!=-1)
    close(tcp_tl->out_socket);
  close(tcp_tl->control_fds[0]);
  close(tcp_tl->control_fds[1]);
#endif
}

int
tcp_transport_layer_set_in_port(tcp_transport_layer_t *tcp_tl, int port)/*��������˿�*/
{
#ifdef OSIP_TCP
  if (tcp_tl==NULL) return -1;
  tcp_tl->in_port = port;
#endif
  return 0;
}

int
tcp_transport_layer_set_out_port(tcp_transport_layer_t *tcp_tl, int port)/*��������˿�*/
{
#ifdef OSIP_TCP
  if (tcp_tl==NULL) return -1;
  tcp_tl->out_port = port;
#endif
  return 0;
}


int tcp_transport_layer_stop(tcp_transport_layer_t *tcp_tl)/*ֹͣTCP�����*/
{
#ifdef OSIP_TCP
  char a=0;
  if (tcp_tl==NULL) return -1;
  if (tcp_tl->control_fds[1])
    write(tcp_tl->control_fds[1],&a,1);
#endif
  return 0;
}

int tcp_transport_layer_close(tcp_transport_layer_t *tcp_tl)/*�ر�TCP�����*/
{
#ifdef OSIP_TCP
  char a=0;
  if (tcp_tl==NULL) return -1;
  if (tcp_tl->control_fds[1])
    write(tcp_tl->control_fds[1],&a,1);
  close(tcp_tl->in_socket);
#endif
  return 0;
}

/* max_analysed = ���������޷��ش�����Ϣʱ�ܹ�����������Ϣ����
-1 ��ʹ��һ����ʼ�ս���
���κ�������㶼��ͨ������tcp_transport_layer_close(tcp_tl);����ֹ�������
��������������صĽ��ͣ�
-2 ����
-1 ��ʱ
0  ����tcp_transport_layer_close(tcp_tl);��������
1  �ﵽmax_analysed*/
int
tcp_transport_layer_execute(tcp_transport_layer_t *tcp_tl, osip_t *osip,/*ִ��TCP�����*/
			    int sec_max, int usec_max, int max_analysed)
{
#ifdef OSIP_TCP
  fd_set osip_fdset;
  char *buf;
  int max_fd;
  struct timeval tv;

  if (tcp_tl==NULL) return -2;
  /* �ȴ����� */
  tv.tv_sec = sec_max;
  tv.tv_usec = usec_max;

  FD_ZERO(&osip_fdset);

  max_fd=tcp_tl->in_socket;
  /* ����һϵ�е��ļ������������ƶ�ջ*/

  if (max_fd<tcp_tl->control_fds[0]) max_fd=tcp_tl->control_fds[0];
  
  FD_SET(tcp_tl->in_socket, &osip_fdset);
  FD_SET(tcp_tl->control_fds[0],&osip_fdset);

  buf = (char *)smalloc(SIP_MESSAGE_MAX_LENGTH*sizeof(char)+1);
  while(1)
    { /* SIP_MESSAGE_MAX_LENGTHӦ�����66001�Ը���Эͬ����*/
      int i;
      max_analysed--;
      if ((sec_max==-1)||(usec_max==-1))
	i = select(max_fd+1, &osip_fdset, NULL, NULL, NULL);
      else
	i = select(max_fd+1, &osip_fdset, NULL, NULL, &tv);
      /* ���ڲ�Ҫ�⿴tvֵ! */
      
      if (0==i)
	{
	  sfree(buf);
	  return -1; /* ����Ϣ����ʱ */
	}
      if (-1==i)
	{
	  sfree(buf);
	  return -2; /* ���� */
	}
      /* �����������ļ���������������߳�if something to read on the control file descriptor, then exit thread*/
      if (FD_ISSET(tcp_tl->control_fds[0],&osip_fdset))
	{
	  OSIP_TRACE(osip_trace(__FILE__,__LINE__,OSIP_INFO1,NULL,"INFO: TCP thread is exiting!\n"));
	  sfree(buf); /* added by AMD 18/08/2001 */
	  return 0;
	}
      
    OSIP_TRACE(osip_trace(__FILE__,__LINE__,OSIP_INFO3,NULL,"INFO: WAITING FOR TCP MESSAGE\n"));
    i = recv(tcp_tl->in_socket, buf, SIP_MESSAGE_MAX_LENGTH ,  0);
    if( i > 0 )
      {
	/* ��Ϣ���ܲ�������"\0"�����������Ǳ���֪�����յ���char������Message might not end with a "\0" but we know the number of */
	/* char received! */
	transaction_t *transaction;
	sipevent_t *sipevent;
	sstrncpy(buf+i,"\0",1);
	TRACE(osip_trace(__FILE__,__LINE__,OSIP_INFO3,NULL,"INFO: RCV TCP MESSAGE\n"));
#ifdef SHOW_MESSAGE
	fprintf(stdout,"%s\n",buf));
	TRACE(osip_trace(__FILE__,__LINE__,OSIP_INFO4,NULL,"%s\n",buf));
#endif
	sipevent = osip_parse(buf);
	transaction = NULL;
	if (sipevent!=NULL)
	  transaction = osip_distribute_event(osip, sipevent);/*osip�����¼�*/

#ifdef OSIP_MT
      if (transaction!=NULL)
	{
	  if (transaction->your_instance==NULL)
	    {
	      transaction_mt_t *transaction_mt;
	      transaction_mt_init(&transaction_mt);
	      transaction_mt_set_transaction(transaction_mt,transaction);
	      transaction_mt_start_transaction(transaction_mt);
	    }
	}
#endif
      if (max_analysed==0) return 1;
      }
    else
      {
      OSIP_TRACE(osip_trace(__FILE__,__LINE__,OSIP_ERROR,NULL,"ERROR: TCP listener failed while receiving data!\n"));
      }
    }
  sfree(buf);/*�ͷ��ڴ�*/
#endif
  return -2;
}
