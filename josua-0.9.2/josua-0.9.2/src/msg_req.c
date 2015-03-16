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

#include <osip/port.h>
#include <osip/dialog.h>

#include <osip/smsg.h>
#include "rcfile.h"

char *register_callid_number = NULL;

/* ��Ҫ�õ���������ʶ��should use cryptographically random identifier is RECOMMENDED.... */
/* by now this should lead to identical call-id when application are
   started at the same time...   */
char *
call_id_new_random()
{
  char *tmp = (char *)smalloc(33);
  unsigned int number = new_random_number();
  sprintf(tmp,"%u",number);
  return tmp;
}

char *
from_tag_new_random()
{
  return call_id_new_random();
}

char *
to_tag_new_random()
{
  return call_id_new_random();
}

unsigned int
via_branch_new_random()
{
  return new_random_number();
}
/* �ڻỰ֮��׼��һ����С����prepare a minimal request (outside of a dialog) with required headers */
/* 
   �����������������("INVITE", "REGISTER"...)method_name is the type of request. ("INVITE", "REGISTER"...)
   to is the remote target URI
   ʹ��TCP����UDP���䶼���ԣ���ǰʹ�õ���UDP��transport is either "TCP" or "UDP" (by now, only UDP is implemented!)
*/
int
generating_request_out_of_dialog(sip_t **dest, char *method_name,
				 char *to, char *transport)
{
  /* Section 8.1:
     A valid request contains at a minimum "To, From, Call-iD, Cseq,
     Max-Forwards and Via
  */
  static int register_cseq_number = 1; /* ע�����Ǵ�1��ʼalways start registration with 1 */
  int i;
  char *max_fwd;
  char *proxy_require;
  sip_t *request;

  if (register_callid_number==NULL)
    register_callid_number = call_id_new_random();

  i = msg_init(&request);
  if (i!=0) return -1;

  /* prepare the request-line׼�������߳� */
  request->strtline->sipmethod  = sgetcopy(method_name);
  request->strtline->sipversion = sgetcopy("SIP/2.0");
  request->strtline->statuscode   = NULL;
  request->strtline->reasonphrase = NULL;

  if (0==strcmp("REGISTER", method_name))
    {
      url_init(&(request->strtline->rquri));
      url_parse(request->strtline->rquri, josua_config_get_element("registrar"));
      msg_setto(request, josua_config_get_element("from"));
    }
  else
    {
      /* ��ע�ᣨREGISTER��״̬�κ�״̬������in any cases except REGISTER: */
      i = msg_setto(request, to);
      if (i!=0)
	{
	  fprintf(stderr, "ERROR: callee address does not seems to be a sipurl: %s\n", to);
	  goto brood_error_1;
	}
      if (josua_config_get_element("outbound_proxy")!=NULL)
	{  /* equal to a pre-existing route set��������·������ */
	   /* if the pre-existing route set contains a "lr" (compliance
	      with bis-08) then the rquri should contains the remote target
	      URI �������·�����ð���һ��"lr"(����bis-08)������������Զ��Ŀ��URI*/
	  url_param_t *lr_param;
	  route_t *o_proxy;
#ifndef __VXWORKS_OS__
	  route_init(&o_proxy);
#else
	  route_init2(&o_proxy);
#endif
	  route_parse(o_proxy, josua_config_get_element("outbound_proxy"));
	  url_uparam_getbyname(o_proxy->url, "lr", &lr_param);
	  if (lr_param!=NULL) /* to is the remote target URI in this case! */
	    {
	      url_clone(request->to->url, &(request->strtline->rquri));
	      /* "[request] MUST includes a Route header field containing
	       the route set values in order." [request]�������һ��·����ͷ�����а�����·��������ֵ*/
	      list_add(request->routes, o_proxy, 0);
	    }
	  else
	    /* if the first URI of route set does not contain "lr", the rquri
	       is set to the first uri of route set 
		   ���·�������õĵ�һ��URI������"lr"�������뱻��Ϊ��URI*/
	    {
	      request->strtline->rquri = o_proxy->url;
	      o_proxy->url = NULL;
	      route_free(o_proxy);
	      sfree(o_proxy);
	      /* add the route set */
	      /* "The UAC MUST add a route header field containing
		 the remainder of the route set values in order.
		 The UAC MUST then place the remote target URI into
		 the route header field as the last value
	       */
	      msg_setroute(request, to);
	    }
	}
      else /* No route set (outbound proxy) is used */
	{
	  /* The UAC must put the remote target URI (to field) in the rquri */
	    i = url_clone(request->to->url, &(request->strtline->rquri));
	    if (i!=0) goto brood_error_1;
	}
    }

  /* set To and From */
  msg_setfrom(request, josua_config_get_element("from"));
  /* add a tag */
  from_set_tag(request->from, from_tag_new_random());
  
  /* set the cseq and call_id header */
  if (0==strcmp("REGISTER", method_name))
    {
      call_id_t *callid;
      cseq_t *cseq;
      char *num;

      /* call-id is always the same for REGISTRATIONS */
      i = call_id_init(&callid);
      if (i!=0) goto brood_error_1;
      call_id_setnumber(callid, sgetcopy(register_callid_number));
      call_id_sethost(callid, sgetcopy(josua_config_get_element("localip")));
      request->call_id = callid;

      i = cseq_init(&cseq);
      if (i!=0) goto brood_error_1;
      num = (char *)smalloc(10); /* should never be more than 10 chars... */
      sprintf(num, "%i", register_cseq_number);
      cseq_setnumber(cseq, num);
      cseq_setmethod(cseq, sgetcopy(method_name));
      request->cseq = cseq;

      register_cseq_number++;
    }
  else
    {
      /* set the call-id */
      call_id_t *callid;
      cseq_t *cseq;
      i = call_id_init(&callid);
      if (i!=0) goto brood_error_1;
      call_id_setnumber(callid, call_id_new_random());
      call_id_sethost(callid, sgetcopy(josua_config_get_element("localip")));
      request->call_id = callid;

      i = cseq_init(&cseq);
      if (i!=0) goto brood_error_1;
      cseq_setnumber(cseq, sgetcopy("20")); /* always start with 20... :-> */
      cseq_setmethod(cseq, sgetcopy(method_name));
      request->cseq = cseq;
    }

  /* always add the Max-Forward header */
  max_fwd = josua_config_get_element("max_forward");
  if (max_fwd==NULL)
    {
      msg_setmax_forward(request, "50"); /* a UA should start a request with 70 */
    }
  else
    {
      msg_setmax_forward(request, max_fwd);
    }

  /* Add Proxy-Require Features */
  proxy_require = josua_config_get_element("proxy_require");
  if (proxy_require!=NULL)
    {
      msg_setproxy_require(request, proxy_require);
    }

  {
    via_t *via;
    char *tmp = (char *)smalloc(90*sizeof(char));
    sprintf(tmp, "SIP/2.0/%s %s:%s;branch=z9hG4bK%u", transport,
	    josua_config_get_element("localip"),
	    josua_config_get_element("localport"),
	    via_branch_new_random() );
    msg_setvia(request, tmp);
    sfree(tmp);
    via = list_get(request->vias, 0);
    if (via==NULL) goto brood_error_1;
    generic_param_add(via->via_params, sgetcopy("rport"), NULL);
  }

  /* add specific headers for each kind of request... */

  if (0==strcmp("INVITE", method_name))
    {
      /* add a Contact header for requests that establish a dialog:
	 (only "INVITE") */
      /* this Contact is the global location where to send request
	 outside of a dialog! like sip:jack@atosc.org? */
      msg_setcontact(request, josua_config_get_element("global_contact"));

      /* Here we'll add the supported header if it's needed! */
      /* the require header must be added by the upper layer if needed */
      if (josua_config_get_element("supported")!=NULL)
	msg_setsupported(request, josua_config_get_element("supported"));
    }
  else if (0==strcmp("REGISTER", method_name))
    {
      char *tmp = josua_config_get_element("contact");
      if (tmp!=NULL)
	{
	  msg_setcontact(request, tmp);
	}
    }
  else if (0==strcmp("INFO", method_name))
    {

    }
  else if (0==strcmp("OPTIONS", method_name))
    {

    }

  msg_setuser_agent(request, "oSIP-ua/0.8.1");
  /*  else if ... */
  *dest = request;
  return 0;

 brood_error_1:
  msg_free(request);
  sfree(request);
  *dest = NULL;
  return -1;
}

int
generating_register(sip_t **reg, char *transport)
{
  int i;
  i = generating_request_out_of_dialog(reg, "REGISTER", NULL, transport);
  if (i!=0) return -1;

  msg_setheader(*reg, "expires", josua_config_get_element("expires"));
  msg_setcontent_length(*reg, "0");
  
  return 0;
}

/* this method can't be called unless the previous
   INVITE transaction is over */
int
generating_initial_invite(sip_t **invite, char *to,
			  char *subject, char *transport,
			  char *sdp)
{
  char *size;
  int i;
  i = generating_request_out_of_dialog(invite, "INVITE", to, transport);
  if (i!=0) return -1;
  
  if (sdp!=NULL)
    {
      size= (char *)smalloc(5*sizeof(char));
      sprintf(size,"%i",strlen(sdp)-50);
      msg_setcontent_length(*invite, size);
      sfree(size);
      
      msg_setbody(*invite, sdp);
      msg_setcontent_type(*invite, "application/sdp");
    }
  else
    {
      msg_setcontent_length(*invite, "0");

    }
  /* About content-length:
     in case a body is added after this method has been called, the
     application MUST take care of removing this header before
     replacing it.
     It should also take care of content-disposition and mime-type headers
  */


  msg_setsubject(*invite, subject);

  msg_setallow(*invite, "INVITE");
  msg_setallow(*invite, "ACK");
  /*  msg_setallow(*invite, "OPTIONS"); */
  msg_setallow(*invite, "CANCEL");
  msg_setallow(*invite, "BYE");

  /* after this delay, we should send a CANCEL */
  if (josua_config_get_element("invite_timeout")!=NULL)
    msg_setexpires(*invite, josua_config_get_element("invite_timeout"));

  if (josua_config_get_element("organization")!=NULL)
    msg_setorganization(*invite, josua_config_get_element("organization"));


  return 0;
}


int
generating_options(sip_t **options, char *to, char *transport, char *sdp)
{
  int i;
  i = generating_request_out_of_dialog(options, "OPTIONS", to, transport);
  if (i!=0) return -1;

  if (sdp!=NULL)
    {
      char *size;
      size= (char *)smalloc(5*sizeof(char));
      sprintf(size,"%i",strlen(sdp));
      msg_setcontent_length(*options, size);
      sfree(size);
      
      msg_setcontent_type(*options, "application/sdp");
      msg_setbody(*options, sdp);
    }
  else
    msg_setcontent_length(*options, "0");
  return 0;
}


int
dialog_fill_route_set(dialog_t *dialog, sip_t *request)
{
  /* if the pre-existing route set contains a "lr" (compliance
     with bis-08) then the rquri should contains the remote target
     URI */
  int i;
  int pos=0;
  url_param_t *lr_param;
  route_t *route;
  char *last_route;
  /* AMD bug: fixed 17/06/2002 */

  if (dialog->type==CALLER)
    {
      pos = list_size(dialog->route_set)-1;
      route = (route_t*)list_get(dialog->route_set, pos);
    }
  else
    route = (route_t*)list_get(dialog->route_set, 0);
    
  url_uparam_getbyname(route->url, "lr", &lr_param);
  if (lr_param!=NULL) /* the remote target URI is the rquri! */
    {
      i = url_clone(dialog->remote_contact_uri->url,
		    &(request->strtline->rquri));
      if (i!=0) return -1;
      /* "[request] MUST includes a Route header field containing
	 the route set values in order." */
      /* AMD bug: fixed 17/06/2002 */
      pos=0; /* first element is at index 0 */
      while (!list_eol(dialog->route_set, pos))
	{
	  route_t *route2;
	  route = list_get(dialog->route_set, pos);
	  i = route_clone(route, &route2);
	  if (i!=0) return -1;
	  if (dialog->type==CALLER)
	    list_add(request->routes, route2, 0);
	  else
	    list_add(request->routes, route2, -1);
	  pos++;
	}
      return 0;
    }

  /* if the first URI of route set does not contain "lr", the rquri
     is set to the first uri of route set */
  
  
  i = url_clone(route->url, &(request->strtline->rquri));
  if (i!=0) return -1;
  /* add the route set */
  /* "The UAC MUST add a route header field containing
     the remainder of the route set values in order. */
  pos=0; /* yes it is */
  
  while (!list_eol(dialog->route_set, pos)) /* not the first one in the list */
    {
      route_t *route2;
      route = list_get(dialog->route_set, pos);
      i = route_clone(route, &route2);
      if (i!=0) return -1;
      if (dialog->type==CALLER)
	{
	  if (pos!=0)
	    list_add(request->routes, route2, 0);
	}
      else
	{
	  if (!list_eol(dialog->route_set, pos+1))
	    list_add(request->routes, route2, -1);
	}
	  pos++;
    }
      /* The UAC MUST then place the remote target URI into
	 the route header field as the last value */
  i = url_2char(dialog->remote_contact_uri->url, &last_route);
  if (i!=0) return -1;
  i = msg_setroute(request, last_route);
  if (i!=0) { sfree(last_route); return -1; }

  
  /* route header and rquri set */
  return 0;
}

int
generating_request_within_dialog(sip_t **dest, char *method_name,
				 dialog_t *dialog, char *transport)
{
  int i;
  sip_t *request;
  char *max_fwd;
  char *proxy_require;

  i = msg_init(&request);
  if (i!=0) return -1;

  if (dialog->remote_contact_uri==NULL)
    {
      /* this dialog is probably not established! or the remote UA
	 is not compliant with the latest RFC
      */
      msg_free(request);
      sfree(request);
      return -1;
    }
  /* prepare the request-line */
  request->strtline->sipmethod  = sgetcopy(method_name);
  request->strtline->sipversion = sgetcopy("SIP/2.0");
  request->strtline->statuscode   = NULL;
  request->strtline->reasonphrase = NULL;

  /* and the request uri???? */
  if (list_eol(dialog->route_set, 0))
    {
      /* The UAC must put the remote target URI (to field) in the rquri */
      i = url_clone(dialog->remote_contact_uri->url, &(request->strtline->rquri));
      if (i!=0) goto grwd_error_1;
    }
  else
    {
      /* fill the request-uri, and the route headers. */
      dialog_fill_route_set(dialog, request);
    }
  
  /* To and From already contains the proper tag! */
  i = to_clone(dialog->remote_uri, &(request->to));
  if (i!=0) goto grwd_error_1;
  i = from_clone(dialog->local_uri, &(request->from));
  if (i!=0) goto grwd_error_1;

  /* set the cseq and call_id header */
  msg_setcall_id(request, dialog->call_id);

  if (0==strcmp("ACK", method_name))
    {
      cseq_t *cseq;
      char *tmp;
      i = cseq_init(&cseq);
      if (i!=0) goto grwd_error_1;
      tmp = smalloc(10);
      sprintf(tmp,"%i", dialog->local_cseq);
      cseq_setnumber(cseq, tmp);
      cseq_setmethod(cseq, sgetcopy(method_name));
      request->cseq = cseq;
    }
  else
    {
      cseq_t *cseq;
      char *tmp;
      i = cseq_init(&cseq);
      if (i!=0) goto grwd_error_1;
      dialog->local_cseq++; /* we should we do that?? */
      tmp = smalloc(10);
      sprintf(tmp,"%i", dialog->local_cseq);
      cseq_setnumber(cseq, tmp);
      cseq_setmethod(cseq, sgetcopy(method_name));
      request->cseq = cseq;
    }
  
  /* always add the Max-Forward header */
  max_fwd = josua_config_get_element("max_forward");
  if (max_fwd==NULL)
    {
      msg_setmax_forward(request, "50"); /* a UA should start a request with 70 */
    }
  else
    {
      msg_setmax_forward(request, max_fwd);
    }

  /* Add Proxy-Require Features */
  proxy_require = josua_config_get_element("proxy_require");
  if (proxy_require!=NULL)
    {
      msg_setproxy_require(request, proxy_require);
    }

  /* even for ACK for 2xx (ACK within a dialog), the branch ID MUST
     be a new ONE! */
  {
    via_t *via;
    char *tmp = (char *)smalloc(90*sizeof(char));
    sprintf(tmp, "SIP/2.0/%s %s:%s;branch=z9hG4bK%u", transport,
	    josua_config_get_element("localip") ,josua_config_get_element("localport"),
	    via_branch_new_random());
    msg_setvia(request, tmp);
    sfree(tmp);
    via = list_get(request->vias, 0);
    if (via==NULL) goto grwd_error_1;
    generic_param_add(via->via_params, sgetcopy("rport"), NULL);
  }

  /* add specific headers for each kind of request... */

  if (0==strcmp("INVITE", method_name))
    {
      /* add a Contact header for requests that establish a dialog:
	 (only "INVITE") */
      /* this Contact is the global location where to send request
	 outside of a dialog! like sip:jack@atosc.org? */
      msg_setcontact(request, josua_config_get_element("global_contact"));

      /* Here we'll add the supported header if it's needed! */
      /* the require header must be added by the upper layer if needed */
      if (josua_config_get_element("supported")!=NULL)
	msg_setsupported(request, josua_config_get_element("supported"));
    }
  else if (0==strcmp("INFO", method_name))
    {

    }
  else if (0==strcmp("OPTIONS", method_name))
    {
      msg_setaccept(request, "application/sdp");
    }
  else if (0==strcmp("ACK", method_name))
    {
      /* The ACK MUST contains the same credential than the INVITE!! */
      /* TODO... */
    }

  msg_setuser_agent(request, "oSIP-ua/0.8.1");
  /*  else if ... */
  *dest = request;
  return 0;

  /* grwd_error_2: */
  dialog->local_cseq--;
 grwd_error_1:
  msg_free(request);
  sfree(request);
  *dest = NULL;
  return -1;
}

/* this request is only build within a dialog!! */
int
generating_invite_within_dialog(sip_t **invite, dialog_t *dialog,
				char *subject, char *sdp)
{
  int i;
  i = generating_request_within_dialog(invite, "INVITE", dialog, "UDP");
  if (i!=0) return -1;

  if (sdp!=NULL)
    {
      char *size;
      size= (char *)smalloc(5*sizeof(char));
      sprintf(size,"%i",strlen(sdp));
      msg_setcontent_length(*invite, size);
      sfree(size);
      
      msg_setcontent_type(*invite, "application/sdp");
      msg_setbody(*invite, sdp);
    }
  else
    {
      msg_setcontent_length(*invite, "0");
    }
  /* About content-length:
     in case a body is added after this method has been called, the
     application MUST take care of removing this header before
     replacing it.
     It should also take care of content-disposition and mime-type headers
  */

  msg_setsubject(*invite, subject);

  msg_setallow(*invite, "INVITE");
  msg_setallow(*invite, "ACK");
  msg_setallow(*invite, "OPTIONS");
  msg_setallow(*invite, "CANCEL");
  msg_setallow(*invite, "BYE");

  /* after this delay, we should send a CANCEL */
  if (josua_config_get_element("invite_timeout")!=NULL)
    msg_setexpires(*invite, josua_config_get_element("invite_timeout"));

  if (josua_config_get_element("organization")!=NULL)
    msg_setorganization(*invite, josua_config_get_element("organization"));
  return 0;
}

/* this request is only build within a dialog!! */
int
generating_bye(sip_t **bye, dialog_t *dialog)
{
  int i;
  i = generating_request_within_dialog(bye, "BYE", dialog, "UDP");
  if (i!=0) return -1;

  msg_setcontent_length(*bye, "0");
  return 0;
}

/* this request can be inside or outside a dialog */
int
generating_options_within_dialog(sip_t **options, dialog_t *dialog, char *sdp)
{
  int i;
  i = generating_request_within_dialog(options, "OPTIONS", dialog, "UDP");
  if (i!=0) return -1;

  if (sdp!=NULL)
    {
      char *size;
      size= (char *)smalloc(5*sizeof(char));
      sprintf(size,"%i",strlen(sdp));
      msg_setcontent_length(*options, size);
      sfree(size);
      
      msg_setcontent_type(*options, "application/sdp");
      msg_setbody(*options, sdp);
    }
  else
    msg_setcontent_length(*options, "0");

  return 0;
}

int
generating_info(sip_t **info, dialog_t *dialog)
{
  int i;
  i = generating_request_within_dialog(info, "INFO", dialog, "UDP");
  if (i!=0) return -1;
  msg_setcontent_length(*info, "0");
  return 0;
}

/* It is RECOMMENDED to only cancel INVITE request */
int
generating_cancel(sip_t **dest, sip_t *request_cancelled)
{
  int i;
  sip_t *request;
  
  i = msg_init(&request);
  if (i!=0) return -1;
  
  /* prepare the request-line */
  request->strtline->sipmethod = sgetcopy("CANCEL");
  request->strtline->sipversion = sgetcopy("SIP/2.0");
  request->strtline->statuscode   = NULL;
  request->strtline->reasonphrase = NULL;

  i = url_clone(request_cancelled->strtline->rquri, &(request->strtline->rquri));
  if (i!=0) goto gc_error_1;
  
  i = to_clone(request_cancelled->to, &(request->to));
  if (i!=0) goto gc_error_1;
  i = from_clone(request_cancelled->from, &(request->from));
  if (i!=0) goto gc_error_1;
  
  /* set the cseq and call_id header */
  i = call_id_clone(request_cancelled->call_id, &(request->call_id));
  if (i!=0) goto gc_error_1;
  i = cseq_clone(request_cancelled->cseq, &(request->cseq));
  if (i!=0) goto gc_error_1;
  sfree(request->cseq->method);
  request->cseq->method = sgetcopy("CANCEL");
  
  /* copy ONLY the top most Via Field (this method is also used by proxy) */
  {
    via_t *via;
    via_t *via2;
    i = msg_getvia(request_cancelled, 0, &via);
    if (i!=0) goto gc_error_1;
    i = via_clone(via, &via2);
    if (i!=0) goto gc_error_1;
    list_add(request->vias, via2, -1);

    if (via2==NULL) goto gc_error_1;
    generic_param_add(via2->via_params, sgetcopy("rport"), NULL);
  }

  /* add the same route-set than in the previous request */
  {
    int pos=0;
    route_t *route;
    route_t *route2;
    while (!list_eol(request_cancelled->routes, pos))
      {
	route = (route_t*) list_get(request_cancelled->routes, pos);
	i = route_clone(route, &route2);
	if (i!=0) goto gc_error_1;
	list_add(request->routes, route2, -1);
	pos++;
      }
  }

  msg_setcontent_length(request, "0");
  msg_setmax_forward(request, "70"); /* a UA should start a request with 70 */
  msg_setuser_agent(request, "oSIP-ua/0.8.1");

  *dest = request;
  return 0;

 gc_error_1:
  msg_free(request);
  sfree(request);
  *dest = NULL;
  return -1;
}


int
generating_ack_for_2xx(sip_t **ack, dialog_t *dialog)
{
  int i;
  i = generating_request_within_dialog(ack, "ACK", dialog, "UDP");
  if (i!=0) return -1;

  return 0;
}
