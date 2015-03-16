#include "stdafx.h"
#include "codedecoder.h"
#include "Display.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define START	0
#define STOP    5
static unsigned char *dithered_image;
static unsigned char ytab[256+16];
static unsigned char utab[128+16];
static unsigned char vtab[128+16];
static unsigned char pixel[256];
	
CDisplay::CDisplay()
{
      image  =NULL;
      hPalPrev=NULL;
      pbmi    =NULL;
}

CDisplay::~CDisplay()
{

}
 unsigned char *refframe[3],*bframe[3],*newframe[3];
 unsigned char *edgeframe[3], *edgeframeorig[3], *exnewframe[3];
 extern unsigned char *oldrefframe[3];
 int verbose,outtype,expand,refidct,trace;
 CFile h263file;
 CString outputname;
 unsigned char *clp;
 int MBC, MBR;
 int horizontal_size,vertical_size,mb_width,mb_height;
 int coded_picture_width, coded_picture_height;
 int chrom_width,chrom_height,blk_cnt;
 int pict_type,newgob,tr_framenum;	
 int matrix_coefficients,source_format;
 extern ldecode *ld,base;
 extern int quant;
 void ditherframe(unsigned char *src[]);

void CDisplay::play_movie(CDC *pDC,CString szFileName)
{
	verbose = 0;
    outtype = T_X11;  //����������
    nDitherType=8;    //ͼ��任���� ÿ����8����
    ld = &base;        
	trace = 0;   
	expand = 0; 
    hDC=pDC->GetSafeHdc();
    unsigned short gusState =START; 
    int loopflag(0);
    int framenum, first;
    if ((base.infile=h263file.Open(szFileName,CFile::modeRead))==0)
	{   //��263�ļ�
		AfxMessageBox("Can not open input file!");
	    return;
	}
    first = 1;
    pDC->SetBkColor(0xffffff);
    pDC->SetTextColor(RGB(255,100,0));
    pDC->TextOut(480,50,"����ͼ����Ϣ");
    CString out;
    int inum(0),pnum(0);
    do
	{
	  if(first!=1)
          h263file.Seek(0l,CFile::begin);//change
      m_getbits.initbits();
      framenum = tr_framenum = 0;
    
      while (m_gethdr.getheader())
	  {
        if (first)//��һ֡
		{
       	  initdecoder();//��ʼ��������
		  if (outtype==T_X11)
		  {
            init_dither(nDitherType);//��ʼ����ʾͼ�������
            init_display();          //��ʼ����ʾ�ĵ�ɫ�壬bmp�ļ�ͷ
		  }
		pDC->TextOut(450,80,szPictureFormat);
        out.Format("��ʼ��������Q=%d   ",quant);
		pDC->TextOut(450,100,out);
        first = 0;
		}
	    //��ʼ����ͼ��
	    m_getpic.getpicture(&framenum);//����һ֡ͼ��
	    out.Format("��������Q=%d    ",quant);
        pDC->TextOut(450,120,out);
	    if(pict_type==0)
		{
		  out="��֡����������:֡��";
		  inum++;
		}
	    else 
		{
		  out="��֡����������:֡��";
		  pnum++;
		}
	    pDC->TextOut(450,140,out);

        framenum++;
	    tr_framenum++;
	  
        dither(refframe);//��ʾ����ͼ��
     
	    if (gusState==STOP) //������ʾ
          break;
	  }
	}
	while(loopflag);//����
    pDC->SetTextColor(RGB(255,255,100));
    out.Format("������ %d ֡,����I֡=%d(F),P֡=%d(F)",framenum,inum,pnum);
    conclusion=out;
    h263file.Close();
    if (outtype==T_X11)//�ͷŵ�����Ŀռ�
      exit_display();
	toDeleteNewSpace();

}
/*****************************************************************************/
//��ʼ��������
void CDisplay::initdecoder()
{
  int i, cc, size;

  if (!(clp=new unsigned char[1024]))
  {
	  AfxMessageBox("new failed!");
      return;
  }
  clp += 384;
  for (i=-384; i<640; i++) //����[0,255]
    clp[i] = (i<0) ? 0 : ((i>255) ? 255 : i);

  matrix_coefficients = 5;

  if (source_format == SF_CIF) 
  {  // ��������Ϊ CIF
    MBC = 22;
    MBR = 18;
	szPictureFormat="ͼ���ʽ��:352*288";
  }
  else if (source_format == SF_QCIF) 
  {   // ��������Ϊ QCIF
    MBC = 11;
    MBR = 9;
  	szPictureFormat="ͼ���ʽ��:176*144";
  }
  else if (source_format == SF_SQCIF)
  {  // ��������Ϊ SQCIF
	MBC = 8;
    MBR = 6;
    szPictureFormat="ͼ���ʽ��:120*96";
  }
  else
    exit(1);

  horizontal_size = MBC*16;
  vertical_size = MBR*16;
  mb_width = horizontal_size/16;
  mb_height = vertical_size/16;
  coded_picture_width = 16*mb_width;
  coded_picture_height = 16*mb_height;
  chrom_width =  coded_picture_width>>1;
  chrom_height = coded_picture_height>>1;
  blk_cnt = 6;

     //��ʼ��֡�洢��
  for (cc=0; cc<3; cc++)
  {
    if (cc==0)
      size = coded_picture_width*coded_picture_height;
    else
      size = chrom_width*chrom_height;
    if (!(refframe[cc] = new unsigned char[size]))
	{
		AfxMessageBox("new failed!");
		return;
	}
    if (!(oldrefframe[cc] =new unsigned char[size]))
	{
		AfxMessageBox("new failed!");
		return;
	}
    if (!(bframe[cc] =new unsigned char[size]))
	{
		AfxMessageBox("new failed!");
		return;
	}
  }
  for (cc=0; cc<3; cc++) 
  {
    if (cc==0) 
	{
      size = (coded_picture_width+64)*(coded_picture_height+64);
      if (!(edgeframeorig[cc] =new unsigned char[size]))
	  {
		  AfxMessageBox("new failed!");
		  return;
	  }
      edgeframe[cc] = edgeframeorig[cc] + (coded_picture_width+64) * 32 + 32;
    }
    else
	{
      size = (chrom_width+32)*(chrom_height+32);
      if (!(edgeframeorig[cc] =new unsigned char[size]))
	  {
		  AfxMessageBox("new failed!");
		  return;
	  }
      edgeframe[cc] = edgeframeorig[cc] + (chrom_width+32) * 16 + 16;
    }
  }
  if (expand) 
  {
    for (cc=0; cc<3; cc++) 
	{
      if (cc==0)
	     size = coded_picture_width*coded_picture_height*4;
      else
         size = chrom_width*chrom_height*4;
      
      if (!(exnewframe[cc] = new unsigned char[size]))
	  {
		  AfxMessageBox("new failed!");
		  return;
	  }
	}
  } 
  m_idct.init_idct();  // IDCT
}
/*****************************************************************************/
void CDisplay::init_dither(int bpp)
{
  int i, v;
  if ( bpp!=8 )
  { 
     AfxMessageBox("unsuported dither type!");
     return;
  }
  bpp/=8;
  int  size;
  size=bpp*coded_picture_width*coded_picture_height;
  if(!(dithered_image = new unsigned char[size]))
  {
	  AfxMessageBox("new failed!");
	  return;
  }

  for (i=-8; i<256+8; i++)
  {
    v = i>>4;
    if (v<1)
      v = 1;
    else if (v>14)
      v = 14;
    ytab[i+8] = v<<4;
  }

  for (i=0; i<128+16; i++)
  {
    v = (i-40)>>4;
    if (v<0)
      v = 0;
    else if (v>3)
      v = 3;
    utab[i] = v<<2;
    vtab[i] = v;
  }

  for (i=0; i<256; i++)
     pixel[i]=i;

}
/*****************************************************************************/
void CDisplay::init_display()//��ʼ����ʾ��������ɫ��
{
  pbmi= (PBITMAPINFO)malloc(sizeof(BITMAPINFOHEADER) + 240 * sizeof(RGBQUAD));
  pbmi->bmiHeader.biSize = (LONG)sizeof(BITMAPINFOHEADER);
  pbmi->bmiHeader.biPlanes = 1;
  pbmi->bmiHeader.biCompression = 0l;
  pbmi->bmiHeader.biSizeImage = 0l;
  pbmi->bmiHeader.biXPelsPerMeter = 0l;
  pbmi->bmiHeader.biYPelsPerMeter = 0l;
  pbmi->bmiHeader.biClrUsed = 240;
  pbmi->bmiHeader.biClrImportant = 240;
  pbmi->bmiHeader.biBitCount = nDitherType;
  pbmi->bmiHeader.biWidth = coded_picture_width ;
  pbmi->bmiHeader.biHeight= coded_picture_height;
  if ( pbmi->bmiHeader.biBitCount==8 )
  {
     // 8 BPP��������ɫ�� 
     LOGPALETTE *plgpl;
     short      *pPalIndex;
     int         crv, cbu, cgu, cgv;
     int         y, u, v;
     int         i;

     plgpl = (LOGPALETTE*) malloc(sizeof(LOGPALETTE) + 240 * sizeof(PALETTEENTRY));
     plgpl->palNumEntries = 240;
     plgpl->palVersion = 0x300;
     pPalIndex=(short *)pbmi->bmiColors;

     // ������� 
     crv = convmat[matrix_coefficients][0];
     cbu = convmat[matrix_coefficients][1];
     cgu = convmat[matrix_coefficients][2];
     cgv = convmat[matrix_coefficients][3];

     for (i=16; i<240; i++)
     {
       // ��ɫ�ռ�ת��
       y = 16*((i>>4)&15) + 8;
       u = 32*((i>>2)&3)  - 48;
       v = 32*(i&3)       - 48;

       y = 76309 * (y - 16); // (255/219)*65536 

       plgpl->palPalEntry[i].peRed   = clp[(y + crv*v + 32768)>>16];
       plgpl->palPalEntry[i].peGreen = clp[(y - cgu*u -cgv*v + 32768)>>16];
       plgpl->palPalEntry[i].peBlue  = clp[(y + cbu*u + 32786)>>16];
       pPalIndex[i]=i;
     }
     hpal = CreatePalette(plgpl);
     free(plgpl);
     hPalPrev=SelectPalette(hDC,hpal,FALSE);
     RealizePalette(hDC);
  }
}
/*****************************************************************************/
void CDisplay::dither(unsigned char *src[])
{
  ditherframe(src);
  SetDIBitsToDevice(hDC,60,60,coded_picture_width,
  	                   coded_picture_height,
		               0,0,0,coded_picture_height,
    		           dithered_image,pbmi,DIB_PAL_COLORS);
  
}
/*****************************************************************************/
void ditherframe(unsigned char *src[])
{
  int i,j;
  int y,u,v;
  unsigned char *py,*pu,*pv,*dst;

  py = src[0];
  pu = src[1];
  pv = src[2];

#ifdef _WIN32
  dst = dithered_image+(coded_picture_height-1)*coded_picture_width;
#else
  dst = dithered_image;
#endif

  for (j=0; j<coded_picture_height; j+=4)
  {
    // line j + 0 
    for (i=0; i<coded_picture_width; i+=4)
    {
      y = *py++;
      u = *pu++ >> 1;
      v = *pv++ >> 1;
      *dst++ = pixel[ytab[y]|utab[u]|vtab[v]];
      y = *py++;
      
      *dst++ = pixel[ytab[y+8]|utab[u+8]|vtab[v+8]];
      y = *py++;
      u = *pu++ >> 1;
      v = *pv++ >> 1;
      *dst++ = pixel[ytab[y+2]|utab[u+2]|vtab[v+2]];
      y = *py++;
      
      *dst++ = pixel[ytab[y+10]|utab[u+10]|vtab[v+10]];
    }
   
    pu -= chrom_width;
    pv -= chrom_width;
  
#ifdef _WIN32
    dst -= 2*coded_picture_width;
#endif

    /* line j + 1 */
    for (i=0; i<coded_picture_width; i+=4)
    {
      y = *py++;
      u = *pu++ >> 1;
      v = *pv++ >> 1;
      *dst++ = pixel[ytab[y+12]|utab[u+12]|vtab[v+12]];
      y = *py++;
      
      *dst++ = pixel[ytab[y+4]|utab[u+4]|vtab[v+4]];
      y = *py++;
      u = *pu++ >> 1;
      v = *pv++ >> 1;
      *dst++ = pixel[ytab[y+14]|utab[u+14]|vtab[v+14]];
      y = *py++;
      
      *dst++ = pixel[ytab[y+6]|utab[u+6]|vtab[v+6]];
    }

#ifdef _WIN32
    dst -= 2*coded_picture_width;
#endif

    // line j + 2 
    for (i=0; i<coded_picture_width; i+=4)
    {
      y = *py++;
      u = *pu++ >> 1;
      v = *pv++ >> 1;
      *dst++ = pixel[ytab[y+3]|utab[u+3]|vtab[v+3]];
      y = *py++;
      
      *dst++ = pixel[ytab[y+11]|utab[u+11]|vtab[v+11]];
      y = *py++;
      u = *pu++ >> 1;
      v = *pv++ >> 1;
      *dst++ = pixel[ytab[y+1]|utab[u+1]|vtab[v+1]];
      y = *py++;
      
      *dst++ = pixel[ytab[y+9]|utab[u+9]|vtab[v+9]];
    }

    pu -= chrom_width;
    pv -= chrom_width;
  
#ifdef _WIN32
    dst -= 2*coded_picture_width;
#endif

    // line j + 3 
    for (i=0; i<coded_picture_width; i+=4)
    {
      y = *py++;
      u = *pu++ >> 1;
      v = *pv++ >> 1;
      *dst++ = pixel[ytab[y+15]|utab[u+15]|vtab[v+15]];
      y = *py++;
     *dst++ = pixel[ytab[y+7]|utab[u+7]|vtab[v+7]];
      y = *py++;
      u = *pu++ >> 1;
      v = *pv++ >> 1;
      *dst++ = pixel[ytab[y+13]|utab[u+13]|vtab[v+13]];
      y = *py++;
      
      *dst++ = pixel[ytab[y+5]|utab[u+5]|vtab[v+5]];
    }

#ifdef _WIN32
    dst -= 2*coded_picture_width;
#endif
  }
}
/*****************************************************************************/

//�ͷ���������ڴ漰��ɫ��
void CDisplay::exit_display()
{
   if (pbmi)
   {
      free(pbmi);
      pbmi=NULL;
   }
   if (hPalPrev)
   {
       SelectPalette(hDC,hPalPrev,FALSE);
       DeleteObject(hpal);
       hPalPrev=NULL;
   }

}
/*****************************************************************************/

void CDisplay::toDeleteNewSpace()
{
    delete []clp;
	delete []dithered_image;
	for(int i=0;i<3;i++)
	{
	   delete []refframe[i];
	   delete []bframe[i];
	   delete []edgeframeorig[i];
	   if(expand) 
	     delete []exnewframe[i];
       delete []oldrefframe[i];
	}

}
