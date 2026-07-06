// file: D3DUtils.cpp

#include "stdafx.h"
#pragma hdrstop

#include "gamefont.h"
#include "d3dutils.h"
#include "EditorShaders11.h"
#include "EditorFont11.h"
#include "du_box.h"
#include "du_sphere.h"
#include "du_sphere_part.h"
#include "du_cone.h"
#include "du_cylinder.h"
#include "Library.h"
#include "EditObject.h"
#include "ui_main.h"

#pragma warning(push)
#pragma warning(disable:4995)
#include "d3dx9.h"
#pragma warning(pop)

CDrawUtilities DU_impl;

#define LINE_DIVISION  32
// for drawing sphere
static Fvector circledef1[LINE_DIVISION];
static Fvector circledef2[LINE_DIVISION];
static Fvector circledef3[LINE_DIVISION];

const u32 boxcolor = D3DCOLOR_RGBA(255,255,255,0);
static const int boxvertcount = 48;
static Fvector boxvert[boxvertcount];

#ifdef _EDITOR
#	define DU_DRAW_RS	EDevice.SetRS
#	define DU_DRAW_SH_C(a,c){EDevice.SetShader(a);EDevice.SetRS(D3DRS_TEXTUREFACTOR,c);}
#	define DU_DRAW_SH(a){EDevice.SetShader(a);EDevice.SetRS(D3DRS_TEXTUREFACTOR,0xFFFFFFFF);}
#else
#	define DU_DRAW_RS	RCache.dbg_SetRS
#	define DU_DRAW_SH_C(sh,c){RCache.set_Shader(sh);	RCache.set_c	("tfactor",float(color_get_R(c))/255.f,float(color_get_G(c))/255.f,float(color_get_B(c))/255.f,float(color_get_A(c))/255.f);}
#	define DU_DRAW_SH(sh){RCache.set_Shader(sh);		RCache.set_c	("tfactor",1,1,1,1);}
#endif

#ifdef _EDITOR
#	define FILL_MODE EDevice.dwFillMode
#	define SHADE_MODE EDevice.dwShadeMode
#	define SCREEN_QUALITY EDevice.m_ScreenQuality
#else
#	define FILL_MODE D3DFILL_SOLID
#	define SHADE_MODE D3DSHADE_GOURAUD
#	define SCREEN_QUALITY 1.f
#endif


// identity box
const u32 identboxcolor = D3DCOLOR_RGBA(255,255,255,0);
static const int identboxwirecount = 24;
static Fvector identboxwire[identboxwirecount] = {
	{-0.5f, -0.5f, -0.5f},	{-0.5f, +0.5f, -0.5f},    	{-0.5f, +0.5f, -0.5f},	{+0.5f, +0.5f, -0.5f},
    {+0.5f, +0.5f, -0.5f},	{+0.5f, -0.5f, -0.5f},		{+0.5f, -0.5f, -0.5f},	{-0.5f, -0.5f, -0.5f},
    {-0.5f, +0.5f, +0.5f},	{+0.5f, +0.5f, +0.5f},		{+0.5f, +0.5f, +0.5f},	{+0.5f, -0.5f, +0.5f},
    {+0.5f, -0.5f, +0.5f},	{-0.5f, -0.5f, +0.5f},    	{-0.5f, -0.5f, +0.5f},	{-0.5f, +0.5f, +0.5f},
    {-0.5f, +0.5f, -0.5f},	{-0.5f, +0.5f, +0.5f},		{+0.5f, +0.5f, -0.5f},	{+0.5f, +0.5f, +0.5f},
    {+0.5f, -0.5f, -0.5f},	{+0.5f, -0.5f, +0.5f},		{-0.5f, -0.5f, -0.5f},	{-0.5f, -0.5f, +0.5f}
};

/*
static const int identboxindexcount = 36;
static const WORD identboxindices[identboxindexcount] = {
	0, 1, 2,   2, 3, 0,
	3, 2, 6,   6, 7, 3,
	6, 4, 5,   6, 5, 7,
    4, 1, 5,   1, 0, 5,
    3, 5, 0,   3, 7, 5,
    1, 4, 6,   1, 6, 2};
static const int identboxindexwirecount = 24;
static const WORD identboxindiceswire[identboxindexwirecount] = {
	0, 1, 	1, 2,
	2, 3, 	3, 0,
	4, 6, 	6, 7,
    7, 5, 	5, 4,
    1, 4, 	2, 6,
    3, 7, 	0, 5};
*/

#define SIGN(x) ((x<0)?-1:1)

typedef xr_vector< FVF::L > FLvertexVec; typedef FLvertexVec::iterator FLvertexIt;

static FLvertexVec 	m_GridPoints;

u32 m_ColorAxis	= 0xff000000;
u32 m_ColorGrid	= 0xff909090;
u32 m_ColorGridTh = 0xffb4b4b4;
u32 m_SelectionRect=D3DCOLOR_RGBA(127,255,127,64);

u32 m_ColorSafeRect = 0xffB040B0;

void SPrimitiveBuffer::CreateFromData(D3DPRIMITIVETYPE _pt, u32 _p_cnt, u32 FVF, LPVOID vertices, u32 _v_cnt, u16* indices, u32 _i_cnt)
{
    if (!HW.pDevice) return; // DX11 mode: no DX9 device to create geometry buffers
	IDirect3DVertexBuffer9*	pVB=0;
	IDirect3DIndexBuffer9*	pIB=0;
	p_cnt				= _p_cnt;
	p_type				= _pt;
	v_cnt				= _v_cnt;
	i_cnt				= _i_cnt;
	u32 stride			= D3DXGetFVFVertexSize(FVF);
	R_CHK(HW.pDevice->CreateVertexBuffer(v_cnt*stride, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &pVB, 0));
	u8* 				bytes;
	R_CHK				(pVB->Lock(0,0,(LPVOID*)&bytes,0));
	FLvertexVec	verts	(v_cnt);
	for (u32 k=0; k<v_cnt; ++k)
		verts[k].set	(((Fvector*)vertices)[k],0xFFFFFFFF);
	Memory.mem_copy		(bytes,&*verts.begin(),v_cnt*stride);
	R_CHK				(pVB->Unlock());
	if (i_cnt){ 
		R_CHK(HW.pDevice->CreateIndexBuffer(i_cnt*sizeof(u16),D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_MANAGED,&pIB,NULL));
		R_CHK			(pIB->Lock(0,0,(LPVOID*)&bytes,0));
		Memory.mem_copy	(bytes,indices,i_cnt*sizeof(u16));
		R_CHK			(pIB->Unlock());
		OnRender.bind	(this,&SPrimitiveBuffer::RenderDIP);
	}else{
		OnRender.bind	(this,&SPrimitiveBuffer::RenderDP);
	}
	pGeom.create		(FVF,pVB,pIB);
}
void SPrimitiveBuffer::Destroy()
{                       
	if (pGeom){
		_RELEASE		(pGeom->vb);
		_RELEASE		(pGeom->ib);
		pGeom.destroy	();
	}
}

void CDrawUtilities::UpdateGrid(int number_of_cell, float square_size, int subdiv){
	m_GridPoints.clear();
// grid
	int m_GridSubDiv[2];
	int m_GridCounts[2];
    Fvector2 m_GridStep;

    m_GridStep.set(square_size,square_size);
	m_GridSubDiv[0] = subdiv;
	m_GridSubDiv[1] = subdiv;
	m_GridCounts[0] = number_of_cell;//iFloor(size/step)*subdiv;
	m_GridCounts[1] = number_of_cell;//iFloor(size/step)*subdiv;

	FVF::L left,right;
	left.p.y = right.p.y = 0;

	for(int thin=0; thin<2; thin++){
		for(int i=-m_GridCounts[0]; i<=m_GridCounts[0]; i++){
			if( (!!thin) != !!(i%m_GridSubDiv[0]) ){
				left.p.z = -m_GridCounts[1]*m_GridStep.y;
				right.p.z = m_GridCounts[1]*m_GridStep.y;
				left.p.x = i*m_GridStep.x;
				right.p.x = left.p.x;
				left.color = (i%m_GridSubDiv[0]) ? m_ColorGrid : m_ColorGridTh;
				right.color = left.color;
				m_GridPoints.push_back( left );
				m_GridPoints.push_back( right );
			}
		}
		for(int i=-m_GridCounts[1]; i<=m_GridCounts[1]; i++){
			if( (!!thin) != !!(i%m_GridSubDiv[1]) ){
				left.p.x = -m_GridCounts[0]*m_GridStep.x;
				right.p.x = m_GridCounts[0]*m_GridStep.x;
				left.p.z = i*m_GridStep.y;
				right.p.z = left.p.z;
				left.color = (i%m_GridSubDiv[1]) ? m_ColorGrid : m_ColorGridTh;
				right.color = left.color;
				m_GridPoints.push_back( left );
				m_GridPoints.push_back( right );
			}
		}
	}
}

void CDrawUtilities::OnDeviceCreate()
{
	EDevice.seqRender.Add			(this,REG_PRIORITY_LOW-1000);

	m_SolidBox.CreateFromData		(D3DPT_TRIANGLELIST,DU_BOX_NUMFACES,		D3DFVF_XYZ|D3DFVF_DIFFUSE,du_box_vertices,			DU_BOX_NUMVERTEX,			du_box_faces,			DU_BOX_NUMFACES*3);
	m_SolidCone.CreateFromData		(D3DPT_TRIANGLELIST,DU_CONE_NUMFACES,		D3DFVF_XYZ|D3DFVF_DIFFUSE,du_cone_vertices,		DU_CONE_NUMVERTEX,			du_cone_faces,			DU_CONE_NUMFACES*3);
	m_SolidSphere.CreateFromData	(D3DPT_TRIANGLELIST,DU_SPHERE_NUMFACES,		D3DFVF_XYZ|D3DFVF_DIFFUSE,du_sphere_vertices,		DU_SPHERE_NUMVERTEX,		du_sphere_faces,		DU_SPHERE_NUMFACES*3);
	m_SolidSpherePart.CreateFromData(D3DPT_TRIANGLELIST,DU_SPHERE_PART_NUMFACES,D3DFVF_XYZ|D3DFVF_DIFFUSE,du_sphere_part_vertices,	DU_SPHERE_PART_NUMVERTEX,	du_sphere_part_faces,	DU_SPHERE_PART_NUMFACES*3);
    m_SolidCylinder.CreateFromData	(D3DPT_TRIANGLELIST,DU_CYLINDER_NUMFACES,	D3DFVF_XYZ|D3DFVF_DIFFUSE,du_cylinder_vertices,	DU_CYLINDER_NUMVERTEX,		du_cylinder_faces,		DU_CYLINDER_NUMFACES*3);
    m_WireBox.CreateFromData		(D3DPT_LINELIST,	DU_BOX_NUMLINES,		D3DFVF_XYZ|D3DFVF_DIFFUSE,du_box_vertices,			DU_BOX_NUMVERTEX,			du_box_lines,			DU_BOX_NUMLINES*2);
	m_WireCone.CreateFromData		(D3DPT_LINELIST,	DU_CONE_NUMLINES,		D3DFVF_XYZ|D3DFVF_DIFFUSE,du_cone_vertices,		DU_CONE_NUMVERTEX,			du_cone_lines,			DU_CONE_NUMLINES*2);
	m_WireSphere.CreateFromData		(D3DPT_LINELIST,	DU_SPHERE_NUMLINES,		D3DFVF_XYZ|D3DFVF_DIFFUSE,du_sphere_verticesl,		DU_SPHERE_NUMVERTEXL,		du_sphere_lines,		DU_SPHERE_NUMLINES*2);
	m_WireSpherePart.CreateFromData	(D3DPT_LINELIST,	DU_SPHERE_PART_NUMLINES,D3DFVF_XYZ|D3DFVF_DIFFUSE,du_sphere_part_vertices,	DU_SPHERE_PART_NUMVERTEX,	du_sphere_part_lines,	DU_SPHERE_PART_NUMLINES*2);
    m_WireCylinder.CreateFromData	(D3DPT_LINELIST,	DU_CYLINDER_NUMLINES,	D3DFVF_XYZ|D3DFVF_DIFFUSE,du_cylinder_vertices,	DU_CYLINDER_NUMVERTEX,		du_cylinder_lines,		DU_CYLINDER_NUMLINES*2);

	for(int i=0;i<LINE_DIVISION;i++){                                
		float angle = M_PI * 2.f * (i / (float)LINE_DIVISION);
        float _sa=_sin(angle), _ca=_cos(angle);
		circledef1[i].x = _ca;
		circledef1[i].y = _sa;
		circledef1[i].z = 0;
		circledef2[i].x = 0;
		circledef2[i].y = _ca;
		circledef2[i].z = _sa;
		circledef3[i].x = _sa;
		circledef3[i].y = 0;
		circledef3[i].z = _ca;
	}
    // initialize identity box
	Fbox bb;
	bb.set(-0.505f,-0.505f,-0.505f, 0.505f,0.505f,0.505f);
	for (int i=0; i<8; i++){
    	Fvector S;
    	Fvector p;
        bb.getpoint(i,p);
        S.set((float)SIGN(p.x),(float)SIGN(p.y),(float)SIGN(p.z));
    	boxvert[i*6+0].set(p);
    	boxvert[i*6+1].set(p.x-S.x*0.25f,p.y,p.z);
    	boxvert[i*6+2].set(p);
    	boxvert[i*6+3].set(p.x,p.y-S.y*0.25f,p.z);
    	boxvert[i*6+4].set(p);
    	boxvert[i*6+5].set(p.x,p.y,p.z-S.z*0.25f);
    }
    // create render stream
	vs_L.create		(FVF::F_L,RCache.Vertex.Buffer(),RCache.Index.Buffer());
    vs_TL.create	(FVF::F_TL,RCache.Vertex.Buffer(),RCache.Index.Buffer());
    vs_LIT.create	(FVF::F_LIT,RCache.Vertex.Buffer(),RCache.Index.Buffer());

	m_Font						= xr_new<CGameFont>("hud_font_small");

    m_axis_object = NULL;
}

void CDrawUtilities::OnDeviceDestroy()
{
	EDevice.seqRender.Remove		(this);
	xr_delete					(m_Font);
    m_SolidBox.Destroy			();
	m_SolidCone.Destroy			();
	m_SolidSphere.Destroy		();
	m_SolidSpherePart.Destroy	();
    m_SolidCylinder.Destroy		();
    m_WireBox.Destroy			();
	m_WireCone.Destroy			();
	m_WireSphere.Destroy		();
	m_WireSpherePart.Destroy	();
    m_WireCylinder.Destroy		();

	vs_L.destroy		();
	vs_TL.destroy		();
	vs_LIT.destroy		();
 	Lib.RemoveEditObject(m_axis_object);
}
//----------------

void CDrawUtilities::DrawSpotLight(const Fvector& p, const Fvector& d, float range, float phi, u32 clr)
{
    Fmatrix T;
	Fvector p1;
    float H,P;
    float da	= PI_MUL_2/LINE_DIVISION;
	float b		= range*_cos(PI_DIV_2-phi/2);
	float a		= range*_sin(PI_DIV_2-phi/2);
    d.getHP		(H,P);
    T.setHPB	(H,P,0);
    T.translate_over(p);
    if (g_bEditorDX11) {
        xr_vector<FVF::L> verts; verts.reserve(LINE_DIVISION*2+2);
        for (float angle=0; angle<PI_MUL_2; angle+=da) {
            p1.x = b*_cos(angle); p1.y = b*_sin(angle); p1.z = a;
            T.transform_tiny(p1);
            verts.push_back({}); verts.back().set(p, clr);
            verts.push_back({}); verts.back().set(p1, clr);
        }
        p1.mad(p,d,range);
        verts.push_back({}); verts.back().set(p, clr);
        verts.push_back({}); verts.back().set(p1, clr);
        HW11.DU_DrawPrim(verts.data(), (u32)verts.size(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        return;
    }
    _VertexStream*	Stream	= &RCache.Vertex;
    u32				vBase;
    FVF::L*	pv	 	= (FVF::L*)Stream->Lock(LINE_DIVISION*2+2,vs_L->vb_stride,vBase);
	for (float angle=0; angle<PI_MUL_2; angle+=da){
        float _sa	=_sin(angle);
        float _ca	=_cos(angle);
		p1.x		= b * _ca;
		p1.y		= b * _sa;
        p1.z		= a;
        T.transform_tiny(p1);
        // fill VB
        pv->set		(p,clr); pv++;
        pv->set		(p1,clr); pv++;
    }
    p1.mad			(p,d,range);
    pv->set			(p,clr); pv++;
    pv->set			(p1,clr); pv++;
    Stream->Unlock	(LINE_DIVISION*2+2,vs_L->vb_stride);
    // and Render it as triangle list
    DU_DRAW_DP		(D3DPT_LINELIST,vs_L,vBase,LINE_DIVISION+1);
}

void CDrawUtilities::DrawDirectionalLight(const Fvector& p, const Fvector& d, float radius, float range, u32 c)
{
    float r=radius*0.71f;
	Fvector R,N,D; D.normalize(d);
	Fmatrix rot;

    N.set		(0,1,0);
	if (_abs(D.y)>0.99f) N.set(1,0,0);
	R.crossproduct(N,D); R.normalize();
	N.crossproduct(D,R); N.normalize();
    rot.set(R,N,D,p);
	float sz=radius+range;

    if (g_bEditorDX11) {
        FVF::L verts[6];
        verts[0].set(0,0,r,  c); rot.transform_tiny(verts[0].p);
        verts[1].set(0,0,sz, c); rot.transform_tiny(verts[1].p);
        verts[2].set(-r,0,r, c); rot.transform_tiny(verts[2].p);
        verts[3].set(-r,0,sz,c); rot.transform_tiny(verts[3].p);
        verts[4].set(r,0,r,  c); rot.transform_tiny(verts[4].p);
        verts[5].set(r,0,sz, c); rot.transform_tiny(verts[5].p);
        HW11.DU_DrawPrim(verts, 6, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        DrawLineSphere(p, radius, c, true);
        return;
    }

	// fill VB
	_VertexStream*	Stream	= &RCache.Vertex;
	u32				vBase;
	FVF::L*	pv	 	= (FVF::L*)Stream->Lock(6,vs_L->vb_stride,vBase);
    pv->set			(0,0,r,		c); rot.transform_tiny(pv->p); pv++;
    pv->set			(0,0,sz,	c); rot.transform_tiny(pv->p); pv++;
    pv->set			(-r,0,r,	c); rot.transform_tiny(pv->p); pv++;
    pv->set			(-r,0,sz,	c); rot.transform_tiny(pv->p); pv++;
    pv->set			(r,0,r,		c); rot.transform_tiny(pv->p); pv++;
    pv->set			(r,0,sz,	c); rot.transform_tiny(pv->p); pv++;
	Stream->Unlock	(6,vs_L->vb_stride);

	// and Render it as triangle list
    DU_DRAW_DP		(D3DPT_LINELIST,vs_L,vBase,3);

    Fbox b;
    b.min.set(-r,-r,-r);
    b.max.set(r,r,r);

	DrawLineSphere	( p, radius, c, true );
}

void CDrawUtilities::DrawPointLight(const Fvector& p, float radius, u32 c)
{
	if (!g_bEditorDX11) RCache.set_xform_world(Fidentity);
	DrawCross(p, radius,radius,radius, radius,radius,radius, c, true);
}

void CDrawUtilities::DrawEntity(u32 clr, ref_shader s, const Fmatrix& xf)
{
    if (g_bEditorDX11) {
        // DU_DrawPrim draws in world space (no per-object world matrix), so the
        // local flag geometry must be transformed here. Callers used to rely on
        // RCache.set_xform_world, which is a no-op on the DX11 editor path.
        static const Fvector lp[5] = {
            {0.f,0.f,0.f},{0.f,1.f,0.f},{0.f,1.f,.5f},{0.f,.5f,.5f},{0.f,.5f,0.f}
        };
        FVF::L verts[5];
        for (int i = 0; i < 5; i++) {
            Fvector w; xf.transform_tiny(w, lp[i]);
            verts[i].set(w, clr);
        }
        HW11.DU_DrawPrim(verts, 5, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

        // Filled flag panel (the icon texture isn't supported on the DX11 DU path,
        // so it is filled flat). Drawn double-sided so it never culls from behind.
        Fvector q[4];
        xf.transform_tiny(q[0], Fvector().set(0.f,1.f,0.f));
        xf.transform_tiny(q[1], Fvector().set(0.f,1.f,.5f));
        xf.transform_tiny(q[2], Fvector().set(0.f,.5f,.5f));
        xf.transform_tiny(q[3], Fvector().set(0.f,.5f,0.f));
        FVF::L fq[12];
        fq[0].set(q[0],clr); fq[1].set(q[1],clr); fq[2].set(q[2],clr);
        fq[3].set(q[0],clr); fq[4].set(q[2],clr); fq[5].set(q[3],clr);
        fq[6].set(q[0],clr); fq[7].set(q[2],clr); fq[8].set(q[1],clr);   // back side
        fq[9].set(q[0],clr); fq[10].set(q[3],clr); fq[11].set(q[2],clr);
        HW11.DU_DrawPrim(fq, 12, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        return;
    }
	// fill VB
	_VertexStream*	Stream	= &RCache.Vertex;
	u32			vBase;
	FVF::L*	pv	 	= (FVF::L*)Stream->Lock(5,vs_L->vb_stride,vBase);
    pv->set			(0.f,0.f,0.f,clr); pv++;
    pv->set			(0.f,1.f,0.f,clr); pv++;
    pv->set			(0.f,1.f,.5f,clr); pv++;
    pv->set			(0.f,.5f,.5f,clr); pv++;
    pv->set			(0.f,.5f,0.f,clr); pv++;
	Stream->Unlock	(5,vs_L->vb_stride);
	// render flagshtok
    DU_DRAW_SH		(EDevice.m_WireShader);
    DU_DRAW_DP		(D3DPT_LINESTRIP,vs_L,vBase,4);

    if (s) DU_DRAW_SH(s);
    {
        // fill VB
        FVF::LIT*	pv	 = (FVF::LIT*)Stream->Lock(6,vs_LIT->vb_stride,vBase);
        pv->set		(0.f,1.f,0.f,clr,0.f,0.f);	pv++;
        pv->set		(0.f,1.f,.5f,clr,1.f,0.f);	pv++;
        pv->set		(0.f,.5f,.5f,clr,1.f,1.f);	pv++;
        pv->set		(0.f,.5f,0.f,clr,0.f,1.f);	pv++;
        pv->set		(0.f,.5f,.5f,clr,1.f,1.f);	pv++;
        pv->set		(0.f,1.f,.5f,clr,1.f,0.f);	pv++;
        Stream->Unlock	(6,vs_LIT->vb_stride);
        // and Render it as line list
        DU_DRAW_DP		(D3DPT_TRIANGLEFAN,vs_LIT,vBase,4);
    }
}

void CDrawUtilities::DrawFlag(const Fvector& p, float heading, float height, float sz, float sz_fl, u32 clr, BOOL bDrawEntity){
    if (g_bEditorDX11) {
        FVF::L stem[2];
        stem[0].set(p, clr);
        stem[1].set(p.x, p.y+height, p.z, clr);
        HW11.DU_DrawPrim(stem, 2, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        float rx = _sin(heading), rz = _cos(heading);
        if (bDrawEntity) {
            float sz2 = sz * 0.8f;
            FVF::L fl[6];
            fl[0].set(p.x,            p.y+height,                           p.z,            clr);
            fl[1].set(p.x+rx*sz2,     p.y+height,                           p.z+rz*sz2,     clr);
            sz2 *= 0.5f;
            fl[2].set(p.x,            p.y+height*(1.f-sz_fl*.5f),            p.z,            clr);
            fl[3].set(p.x+rx*sz2*0.6f,p.y+height*(1.f-sz_fl*.5f),           p.z+rz*sz2*0.75f,clr);
            fl[4].set(p.x,            p.y+height*(1.f-sz_fl),               p.z,            clr);
            fl[5].set(p.x+rx*sz2,     p.y+height*(1.f-sz_fl),               p.z+rz*sz2,     clr);
            HW11.DU_DrawPrim(fl, 6, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        } else {
            FVF::L fl[6];
            fl[0].set(p.x,             p.y+height*(1.f-sz_fl), p.z,            clr);
            fl[1].set(p.x,             p.y+height,             p.z,            clr);
            fl[2].set(p.x+rx*sz, (fl[0].p.y+fl[1].p.y)*0.5f,  p.z+rz*sz,      clr);
            fl[3] = fl[0]; fl[4] = fl[2]; fl[5] = fl[1];
            HW11.DU_DrawPrim(fl, 6, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
        return;
    }
	// fill VB
	_VertexStream*	Stream	= &RCache.Vertex;
	u32			vBase;
	FVF::L*	pv	 	= (FVF::L*)Stream->Lock(2,vs_L->vb_stride,vBase);
    pv->set			(p,clr); pv++;
    pv->set			(p.x,p.y+height,p.z,clr); pv++;
	Stream->Unlock	(2,vs_L->vb_stride);
	// and Render it as triangle list
    DU_DRAW_DP		(D3DPT_LINELIST,vs_L,vBase,1);

    if (bDrawEntity){
		// fill VB
        float rx		= _sin(heading);
        float rz		= _cos(heading);
		FVF::L*	pv	 	= (FVF::L*)Stream->Lock(6,vs_L->vb_stride,vBase);
        sz				*= 0.8f;
        pv->set			(p.x,p.y+height,p.z,clr);											pv++;
        pv->set			(p.x+rx*sz,p.y+height,p.z+rz*sz,clr);                               pv++;
        sz				*= 0.5f;
        pv->set			(p.x,p.y+height*(1.f-sz_fl*.5f),p.z,clr);                           pv++;
        pv->set			(p.x+rx*sz*0.6f,p.y+height*(1.f-sz_fl*.5f),p.z+rz*sz*0.75f,clr);   	pv++;
        pv->set			(p.x,p.y+height*(1.f-sz_fl),p.z,clr);                               pv++;
        pv->set			(p.x+rx*sz,p.y+height*(1.f-sz_fl),p.z+rz*sz,clr);                   pv++;
		Stream->Unlock	(6,vs_L->vb_stride);
		// and Render it as line list
    	DU_DRAW_DP		(D3DPT_LINELIST,vs_L,vBase,3);
    }else{
		// fill VB
		FVF::L*	pv	 	= (FVF::L*)Stream->Lock(6,vs_L->vb_stride,vBase);
	    pv->set			(p.x,p.y+height*(1.f-sz_fl),p.z,clr); 								pv++;
    	pv->set			(p.x,p.y+height,p.z,clr); 											pv++;
	    pv->set			(p.x+_sin(heading)*sz,((pv-2)->p.y+(pv-1)->p.y)/2,p.z+_cos(heading)*sz,clr); pv++;
    	pv->set			(*(pv-3)); 															pv++;
	    pv->set			(*(pv-2)); 															pv++;
    	pv->set			(*(pv-4)); 															pv++;
		Stream->Unlock	(6,vs_L->vb_stride);
		// and Render it as triangle list
    	DU_DRAW_DP		(D3DPT_TRIANGLELIST,vs_L,vBase,2);
    }
}

//------------------------------------------------------------------------------

void CDrawUtilities::DrawRomboid(const Fvector& p, float r, u32 c)
{
static const u32 IL[24]={0,2, 2,5, 0,5, 3,5, 3,0, 4,3, 4,0, 4,2, 1,2, 1,5, 1,3, 1,4};
static const u32 IT[24]={2,4,0, 4,3,0, 3,5,0, 5,2,0, 4,2,1, 2,5,1, 5,3,1, 3,4,1};
static Fvector PT [6] = {
    {0.0f,	1.0f,	0.0f},
    {0.0f,	-1.0f,	0.0f},
    {0.0f,	0.0f,	-1.0f},
    {0.0f,	0.0f,	1.0f},
    {-1.0f,	0.0f,	0.0f},
    {1.0f,	0.0f,	0.0f},
};
    Fcolor C;
    C.set			(c);
    C.mul_rgb		(0.75);
    u32 c1 =		C.get();

	DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 	8,  p, PT, 6, IT, 24, c1, r);
	DrawIndexedPrimitive(D3DPT_LINELIST, 		12, p, PT, 6, IL, 24, c, r);
	return;

    u32 vBase, iBase;
    int k;
    FVF::L*	pv;
    WORD* i;
	_VertexStream*	Stream	= &RCache.Vertex;
	_IndexStream*	StreamI	= &RCache.Index;

	// fill VB
	pv	 			= (FVF::L*)Stream->Lock(6,vs_L->vb_stride,vBase);
    pv->set			(p.x,	p.y+r,	p.z,	c1); pv++;
    pv->set			(p.x,	p.y-r,	p.z,	c1); pv++;
    pv->set			(p.x,	p.y,	p.z-r,	c1); pv++;
    pv->set			(p.x,	p.y,	p.z+r,	c1); pv++;
    pv->set			(p.x-r,	p.y,	p.z,	c1); pv++;
    pv->set			(p.x+r,	p.y,	p.z,	c1); pv++;
	Stream->Unlock	(6,vs_L->vb_stride);

    i 				= StreamI->Lock(24,iBase);
    for (k=0; k<24; k++,i++) *i=IT[k];
    StreamI->Unlock(24);

	// and Render it as triangle list
	DU_DRAW_DIP		(D3DPT_TRIANGLELIST,vs_L,vBase,0,6, iBase,12);

    // draw lines
	pv	 			= (FVF::L*)Stream->Lock(6,vs_L->vb_stride,vBase);
    pv->set			(p.x,	p.y+r,	p.z,	c); pv++;
    pv->set			(p.x,	p.y-r,	p.z,	c); pv++;
    pv->set			(p.x,	p.y,	p.z-r,	c); pv++;
    pv->set			(p.x,	p.y,	p.z+r,	c); pv++;
    pv->set			(p.x-r,	p.y,	p.z,	c); pv++;
    pv->set			(p.x+r,	p.y,	p.z,	c); pv++;
	Stream->Unlock	(6,vs_L->vb_stride);

    i 				= StreamI->Lock(24,iBase);
    for (k=0; k<24; k++,i++) *i=IL[k];
    StreamI->Unlock	(24);

	DU_DRAW_DIP		(D3DPT_LINELIST,vs_L,vBase,0,6, iBase,12);
   
}
//------------------------------------------------------------------------------

void CDrawUtilities::DrawSound(const Fvector& p, float r, u32 c){
	DrawCross(p, r,r,r, r,r,r, c, true);
}
//------------------------------------------------------------------------------
void CDrawUtilities::DrawIdentCone	(BOOL bSolid, BOOL bWire, u32 clr_s, u32 clr_w)
{
    if (bWire){
        DU_DRAW_SH_C		(EDevice.m_WireShader, clr_w);
    	m_WireCone.Render	();
    }
    if (bSolid){
        DU_DRAW_SH_C		(color_get_A(clr_s)>=254?EDevice.m_WireShader:EDevice.m_SelectionShader,	clr_s);
    	m_SolidCone.Render	();
    }
	DU_DRAW_RS	(D3DRS_TEXTUREFACTOR,	0xffffffff);
}

void CDrawUtilities::DrawIdentSphere	(BOOL bSolid, BOOL bWire, u32 clr_s, u32 clr_w)
{
    if (g_bEditorDX11) return; // requires world xform via RCache; callers must use DX11-specific paths
    if (bWire){
        DU_DRAW_SH_C	(EDevice.m_WireShader,clr_w);
     	m_WireSphere.Render	();
    }
    if (bSolid){
        DU_DRAW_SH_C	(color_get_A(clr_s)>=254?EDevice.m_WireShader:EDevice.m_SelectionShader,clr_s);
    	m_SolidSphere.Render();
    }
	DU_DRAW_RS	(D3DRS_TEXTUREFACTOR,	0xffffffff);
}

void CDrawUtilities::DrawIdentSpherePart(BOOL bSolid, BOOL bWire, u32 clr_s, u32 clr_w)
{
    if (bWire){
        DU_DRAW_SH_C	(EDevice.m_WireShader,clr_w);
     	m_WireSpherePart.Render	();
    }
    if (bSolid){
        DU_DRAW_SH_C	(color_get_A(clr_s)>=254?EDevice.m_WireShader:EDevice.m_SelectionShader,clr_s);
    	m_SolidSpherePart.Render();
    }
	DU_DRAW_RS	(D3DRS_TEXTUREFACTOR,	0xffffffff);
}

void CDrawUtilities::DrawIdentCylinder	(BOOL bSolid, BOOL bWire, u32 clr_s, u32 clr_w)
{
    if (bWire){
        DU_DRAW_SH_C	(EDevice.m_WireShader,clr_w);
    	m_WireCylinder.Render	();
    }
    if (bSolid){
        DU_DRAW_SH_C	(color_get_A(clr_s)>=254?EDevice.m_WireShader:EDevice.m_SelectionShader,clr_s);
    	m_SolidCylinder.Render	();
    }
	DU_DRAW_RS	(D3DRS_TEXTUREFACTOR,	0xffffffff);
}

void CDrawUtilities::DrawIdentBox(BOOL bSolid, BOOL bWire, u32 clr_s, u32 clr_w)
{
    if (g_bEditorDX11) return; // requires world xform via RCache; callers must use DX11-specific paths
    if (bWire){
        DU_DRAW_SH_C	(EDevice.m_WireShader,clr_w);
    	m_WireBox.Render	();
    }
    if (bSolid){
        DU_DRAW_SH_C	(color_get_A(clr_s)>=254?EDevice.m_WireShader:EDevice.m_SelectionShader,clr_s);
    	m_SolidBox.Render	();
    }
	DU_DRAW_RS	(D3DRS_TEXTUREFACTOR,	0xffffffff);
}

void CDrawUtilities::DrawLineSphere(const Fvector& p, float radius, u32 c, BOOL bCross)
{
    if (g_bEditorDX11) {
        const float da = PI_MUL_2 / LINE_DIVISION;
        FVF::L verts[LINE_DIVISION + 1];
        for (int seg = 0; seg < 3; seg++) {
            for (int i = 0; i < LINE_DIVISION; i++) {
                float angle = i * da;
                float sa = _sin(angle), ca = _cos(angle);
                switch (seg) {
                case 0: verts[i].set(p.x + ca*radius, p.y + sa*radius, p.z,            c); break;
                case 1: verts[i].set(p.x,             p.y + ca*radius, p.z + sa*radius, c); break;
                default:verts[i].set(p.x + sa*radius, p.y,             p.z + ca*radius, c); break;
                }
            }
            verts[LINE_DIVISION] = verts[0];
            HW11.DU_DrawPrim(verts, LINE_DIVISION + 1, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        }
        if (bCross) DrawCross(p, radius, radius, radius, radius, radius, radius, c);
        return;
    }
	// fill VB
	_VertexStream*	Stream	= &RCache.Vertex;
	u32			vBase;
    int i;
	FVF::L*	pv;
    // seg 0
	pv	 			= (FVF::L*)Stream->Lock(LINE_DIVISION+1,vs_L->vb_stride,vBase);
	for( i=0; i<LINE_DIVISION; i++,pv++){ pv->p.mad(p,circledef1[i],radius); pv->color=c;}
    pv->set(*(pv-LINE_DIVISION));
	Stream->Unlock	(LINE_DIVISION+1,vs_L->vb_stride);
	DU_DRAW_DP		(D3DPT_LINESTRIP,vs_L,vBase,LINE_DIVISION);
    // seg 1
	pv	 			= (FVF::L*)Stream->Lock(LINE_DIVISION+1,vs_L->vb_stride,vBase);
	for( i=0; i<LINE_DIVISION; i++){ pv->p.mad(p,circledef2[i],radius); pv->color=c; pv++; }
    pv->set(*(pv-LINE_DIVISION)); pv++;
	Stream->Unlock	(LINE_DIVISION+1,vs_L->vb_stride);
	DU_DRAW_DP		(D3DPT_LINESTRIP,vs_L,vBase,LINE_DIVISION);
    // seg 2
	pv	 			= (FVF::L*)Stream->Lock(LINE_DIVISION+1,vs_L->vb_stride,vBase);
	for( i=0; i<LINE_DIVISION; i++){ pv->p.mad(p,circledef3[i],radius); pv->color=c; pv++; }
    pv->set(*(pv-LINE_DIVISION)); pv++;
	Stream->Unlock	(LINE_DIVISION+1,vs_L->vb_stride);
	DU_DRAW_DP		(D3DPT_LINESTRIP,vs_L,vBase,LINE_DIVISION);

    if (bCross) DrawCross(p, radius,radius,radius, radius,radius,radius, c);
}

//----------------------------------------------------
#ifdef _EDITOR
IC float 				_x2real			(float x)
{ return (x+1)*EDevice.m_RenderWidth_2;	}
IC float 				_y2real			(float y)
{ return (y+1)*EDevice.m_RenderHeight_2;}
#else
IC float 				_x2real			(float x)
{ return (x+1)*EDevice.dwWidth*0.5f;	}
IC float 				_y2real			(float y)
{ return (y+1)*EDevice.dwHeight*0.5f;}
#endif

void CDrawUtilities::dbgDrawPlacement(const Fvector& p, int sz, u32 clr, LPCSTR caption, u32 clr_font)
{
	VERIFY( EDevice.b_is_Ready );
    Fvector c;
	float w	= p.x*EDevice.mFullTransform._14 + p.y*EDevice.mFullTransform._24 + p.z*EDevice.mFullTransform._34 + EDevice.mFullTransform._44;
    if (w<0) return; // culling

	float s = (float)sz;
	EDevice.mFullTransform.transform(c,p);

    if (g_bEditorDX11) {
        // c.x/c.y from mFullTransform.transform: NDC +1=right/top, same convention as DX11
        float ndx = c.x;
        float ndy = c.y;
        float px = s * 2.f / (float)HW11.BackBufferW;
        float py = s * 2.f / (float)HW11.BackBufferH;
        FVF::L verts[5];
        verts[0].set(ndx-px, ndy-py, 0.f, clr);
        verts[1].set(ndx+px, ndy-py, 0.f, clr);
        verts[2].set(ndx+px, ndy+py, 0.f, clr);
        verts[3].set(ndx-px, ndy+py, 0.f, clr);
        verts[4] = verts[0];
        HW11.DU_DrawPrim2D(verts, 5, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        return;
    }

	c.x = (float)iFloor(_x2real(c.x)); c.y = (float)iFloor(_y2real(-c.y));

	_VertexStream*	Stream	= &RCache.Vertex;
    u32 vBase;
	FVF::TL* pv	= (FVF::TL*)Stream->Lock(5,vs_TL->vb_stride,vBase);
	pv->p.set(c.x-s,c.y-s,0,1); pv->color=clr; pv++;
	pv->p.set(c.x+s,c.y-s,0,1); pv->color=clr; pv++;
	pv->p.set(c.x+s,c.y+s,0,1); pv->color=clr; pv++;
	pv->p.set(c.x-s,c.y+s,0,1); pv->color=clr; pv++;
	pv->p.set(c.x-s,c.y-s,0,1); pv->color=clr; pv++;
	Stream->Unlock(5,vs_TL->vb_stride);

	// Render it as line strip
    DU_DRAW_DP		(D3DPT_LINESTRIP,vs_TL,vBase,4);
    if (caption){
	    m_Font->SetColor(clr_font);
    	m_Font->Out(c.x,c.y+s,"%s",caption);
    }
}

void CDrawUtilities::dbgDrawVert(const Fvector& p0, u32 clr, LPCSTR caption)
{
	dbgDrawPlacement(p0,1,clr,caption);
	DrawCross		(p0,0.01f,0.01f,0.01f, 0.01f,0.01f,0.01f, clr,false);
}

void CDrawUtilities::dbgDrawEdge(const Fvector& p0,	const Fvector& p1, u32 clr, LPCSTR caption)
{
	dbgDrawPlacement(p0,1,clr,caption);
	DrawCross		(p0,0.01f,0.01f,0.01f, 0.01f,0.01f,0.01f, clr,false);
	DrawCross		(p1,0.01f,0.01f,0.01f, 0.01f,0.01f,0.01f, clr,false);
    DrawLine		(p0,p1,clr);
}

void CDrawUtilities::dbgDrawFace(const Fvector& p0,	const Fvector& p1, const Fvector& p2, u32 clr, LPCSTR caption)
{
	dbgDrawPlacement(p0,1,clr,caption);
	DrawCross		(p0,0.01f,0.01f,0.01f, 0.01f,0.01f,0.01f, clr,false);
	DrawCross		(p1,0.01f,0.01f,0.01f, 0.01f,0.01f,0.01f, clr,false);
	DrawCross		(p2,0.01f,0.01f,0.01f, 0.01f,0.01f,0.01f, clr,false);
    DrawLine		(p0,p1,clr);
    DrawLine		(p1,p2,clr);
    DrawLine		(p2,p0,clr);
}
//----------------------------------------------------

void CDrawUtilities::DrawLine(const Fvector& p0, const Fvector& p1, u32 c){
    if (g_bEditorDX11) {
        FVF::L verts[2]; verts[0].set(p0, c); verts[1].set(p1, c);
        HW11.DU_DrawPrim(verts, 2, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        return;
    }
	// fill VB
	_VertexStream*	Stream	= &RCache.Vertex;
	u32			vBase;
	FVF::L*	pv	 	= (FVF::L*)Stream->Lock(2,vs_L->vb_stride,vBase);
    pv->set			(p0,c); pv++;
    pv->set			(p1,c); pv++;
	Stream->Unlock	(2,vs_L->vb_stride);
	// and Render it as triangle list
    DU_DRAW_DP		(D3DPT_LINELIST,vs_L,vBase,1);
}

void  CDrawUtilities::DrawSelectionBoxB(const Fbox& box, u32* c)
{
    Fvector S,C;
    box.getsize(S);
    box.getcenter(C);
    DrawSelectionBox(C,S,c);
}

//----------------------------------------------------
void CDrawUtilities::DrawSelectionBox(const Fvector& C, const Fvector& S, u32* clr)
{
    u32 cc=(clr)?*clr:boxcolor;
    if (g_bEditorDX11) {
        FVF::L verts[identboxwirecount];
        for (int i=0; i<identboxwirecount; i++) {
            verts[i].p.set(identboxwire[i].x*S.x + C.x,
                           identboxwire[i].y*S.y + C.y,
                           identboxwire[i].z*S.z + C.z);
            verts[i].color = cc;
        }
        HW11.DU_DrawPrim(verts, identboxwirecount, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        return;
    }
	// fill VB
	_VertexStream*	Stream	= &RCache.Vertex;
	u32			vBase;
	FVF::L*	pv	 	= (FVF::L*)Stream->Lock(boxvertcount,vs_L->vb_stride,vBase);
    for (int i=0; i<boxvertcount; i++,pv++){
	    pv->p.mul(boxvert[i],S);
        pv->p.add(C);
        pv->color	= cc;
    }
	Stream->Unlock	(boxvertcount,vs_L->vb_stride);

	// and Render it as triangle list
	DU_DRAW_RS	(D3DRS_FILLMODE,D3DFILL_SOLID);
    DU_DRAW_DP	(D3DPT_LINELIST,vs_L,vBase,boxvertcount/2);
    DU_DRAW_RS	(D3DRS_FILLMODE,FILL_MODE);
}

void CDrawUtilities::DrawBox(const Fvector& offs, const Fvector& Size, BOOL bSolid, BOOL bWire, u32 clr_s, u32 clr_w)
{
    if (g_bEditorDX11) {
        if (bWire) {
            FVF::L verts[identboxwirecount];
            for (int i=0; i<identboxwirecount; i++) {
                verts[i].p.set(identboxwire[i].x*Size.x*2.f + offs.x,
                               identboxwire[i].y*Size.y*2.f + offs.y,
                               identboxwire[i].z*Size.z*2.f + offs.z);
                verts[i].color = clr_w;
            }
            HW11.DU_DrawPrim(verts, identboxwirecount, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        }
        return;
    }
	_VertexStream*	Stream	= &RCache.Vertex;
    if (bWire){
        u32			vBase;
		FVF::L*	pv	 	= (FVF::L*)Stream->Lock(identboxwirecount,vs_L->vb_stride,vBase);
        for (int i=0; i<identboxwirecount; i++, pv++){
            pv->p.mul	(identboxwire[i],Size);
            pv->p.mul	(2);
            pv->p.add	(offs);
            pv->color	= clr_w;
        }
		Stream->Unlock(identboxwirecount,vs_L->vb_stride);

	    DU_DRAW_DP		(D3DPT_LINELIST,vs_L,vBase,identboxwirecount/2);
    }
    if (bSolid){
        u32			vBase;
		FVF::L*	pv	 	= (FVF::L*)Stream->Lock(DU_BOX_NUMVERTEX2,vs_L->vb_stride,vBase);
        for (int i=0; i<DU_BOX_NUMVERTEX2; i++, pv++){
            pv->p.mul	(du_box_vertices2[i],Size);
            pv->p.mul	(2);
            pv->p.add	(offs);
            pv->color	= clr_s;
        }
		Stream->Unlock(DU_BOX_NUMVERTEX2,vs_L->vb_stride);

	    DU_DRAW_DP		(D3DPT_TRIANGLELIST,vs_L,vBase,DU_BOX_NUMFACES);
    }
}
//----------------------------------------------------

void CDrawUtilities::DrawOBB(const Fmatrix& parent, const Fobb& box, u32 clr_s, u32 clr_w)
{
    Fmatrix			R,S,X;
    box.xform_get	(R);
    S.scale			(box.m_halfsize.x*2.f,box.m_halfsize.y*2.f,box.m_halfsize.z*2.f);
    X.mul_43		(R,S);
    R.mul_43		(parent,X);
    if (g_bEditorDX11) {
        // Transform the 8 unit-cube corners by R and draw 12 wireframe edges
        static const Fvector corners[8] = {
            {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f, 0.5f,-0.5f},{-0.5f, 0.5f,-0.5f},
            {-0.5f,-0.5f, 0.5f},{0.5f,-0.5f, 0.5f},{0.5f, 0.5f, 0.5f},{-0.5f, 0.5f, 0.5f}
        };
        static const int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0}, // back face
            {4,5},{5,6},{6,7},{7,4}, // front face
            {0,4},{1,5},{2,6},{3,7}  // connecting edges
        };
        FVF::L verts[24];
        for (int i = 0; i < 12; i++) {
            R.transform_tiny(verts[i*2+0].p, corners[edges[i][0]]);
            verts[i*2+0].color = clr_w;
            R.transform_tiny(verts[i*2+1].p, corners[edges[i][1]]);
            verts[i*2+1].color = clr_w;
        }
        HW11.DU_DrawPrim(verts, 24, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        return;
    }
	RCache.set_xform_world(R);
	DrawIdentBox	(true,true,clr_s,clr_w);
}
//----------------------------------------------------

void CDrawUtilities::DrawAABB(const Fmatrix& parent, const Fvector& center, const Fvector& size, u32 clr_s, u32 clr_w, BOOL bSolid, BOOL bWire)
{
    if (g_bEditorDX11) {
        if (bWire) {
            FVF::L verts[identboxwirecount];
            for (int i=0; i<identboxwirecount; i++) {
                Fvector lp;
                lp.set(identboxwire[i].x*size.x*2.f + center.x,
                       identboxwire[i].y*size.y*2.f + center.y,
                       identboxwire[i].z*size.z*2.f + center.z);
                parent.transform_tiny(verts[i].p, lp);
                verts[i].color = clr_w;
            }
            HW11.DU_DrawPrim(verts, identboxwirecount, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        }
        return;
    }
    Fmatrix			R,S;
    S.scale			(size.x*2.f,size.y*2.f,size.z*2.f);
    S.translate_over(center);
    R.mul_43		(parent,S);
	RCache.set_xform_world(R);
	DrawIdentBox	(bSolid,bWire,clr_s,clr_w);
}

void CDrawUtilities::DrawAABB(const Fvector& p0, const Fvector& p1, u32 clr_s, u32 clr_w, BOOL bSolid, BOOL bWire)
{
    if (g_bEditorDX11) {
        if (bWire) {
            Fvector C; C.set((p1.x+p0.x)*0.5f,(p1.y+p0.y)*0.5f,(p1.z+p0.z)*0.5f);
            Fvector S; S.set(_abs(p1.x-p0.x),_abs(p1.y-p0.y),_abs(p1.z-p0.z));
            FVF::L verts[identboxwirecount];
            for (int i=0; i<identboxwirecount; i++) {
                verts[i].p.set(identboxwire[i].x*S.x + C.x,
                               identboxwire[i].y*S.y + C.y,
                               identboxwire[i].z*S.z + C.z);
                verts[i].color = clr_w;
            }
            HW11.DU_DrawPrim(verts, identboxwirecount, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        }
        return;
    }
    Fmatrix			R;
	Fvector	C; C.set((p1.x+p0.x)*0.5f,(p1.y+p0.y)*0.5f,(p1.z+p0.z)*0.5f);
    R.scale			(_abs(p1.x-p0.x),_abs(p1.y-p0.y),_abs(p1.z-p0.z));
    R.translate_over(C);
	RCache.set_xform_world(R);
	DrawIdentBox	(bSolid,bWire,clr_s,clr_w);
}

void CDrawUtilities::DrawSphere(const Fmatrix& parent, const Fvector& center, float radius, u32 clr_s, u32 clr_w, BOOL bSolid, BOOL bWire)
{
    if (g_bEditorDX11) {
        if (bWire) {
            Fvector wc; parent.transform_tiny(wc, center);
            DrawLineSphere(wc, radius, clr_w, false);
        }
        return;
    }
    Fmatrix B;
    B.scale				(radius,radius,radius);
    B.translate_over	(center);
    B.mulA_43			(parent);
    RCache.set_xform_world(B);
    DrawIdentSphere		(bSolid, bWire, clr_s,clr_w);
}
//----------------------------------------------------

void CDrawUtilities::DrawFace(const Fvector& p0, const Fvector& p1, const Fvector& p2, u32 clr_s, u32 clr_w, BOOL bSolid, BOOL bWire)
{
    if (g_bEditorDX11) {
        if (bSolid) {
            FVF::L tv[3]; tv[0].set(p0,clr_s); tv[1].set(p1,clr_s); tv[2].set(p2,clr_s);
            HW11.DU_DrawPrim(tv, 3, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
        if (bWire) {
            FVF::L lv[4]; lv[0].set(p0,clr_w); lv[1].set(p1,clr_w); lv[2].set(p2,clr_w); lv[3]=lv[0];
            HW11.DU_DrawPrim(lv, 4, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        }
        return;
    }
	_VertexStream*	Stream	= &RCache.Vertex;

    u32				vBase;
    if (bSolid)
    {
        FVF::L*	pv	 	= (FVF::L*)Stream->Lock(3,vs_L->vb_stride,vBase);
        pv->set			(p0,clr_s); pv++;
        pv->set			(p1,clr_s); pv++;
        pv->set			(p2,clr_s); pv++;
        Stream->Unlock	(3,vs_L->vb_stride);
	    DU_DRAW_DP		(D3DPT_TRIANGLELIST,vs_L,vBase,1);
    }
    if (bWire)
    {
        FVF::L*	pv	 	= (FVF::L*)Stream->Lock(4,vs_L->vb_stride,vBase);
        pv->set			(p0,clr_w); pv++;
        pv->set			(p1,clr_w); pv++;
        pv->set			(p2,clr_w); pv++;
        pv->set			(p0,clr_w); pv++;
        Stream->Unlock	(4,vs_L->vb_stride);
	    DU_DRAW_DP		(D3DPT_LINESTRIP,vs_L,vBase,3);
    }
}
//----------------------------------------------------

static const u32 MAX_VERT_COUNT 	= 0xFFFF;
static FVF::L s_dd_verts_dx11[MAX_VERT_COUNT]; // backing store for DX11 face accumulation

void CDrawUtilities::DD_DrawFace_begin(BOOL bWire)
{
	VERIFY				(m_DD_pv_start==0);
    m_DD_wire			= bWire;
    if (g_bEditorDX11) {
        m_DD_pv_start = m_DD_pv = s_dd_verts_dx11;
        return;
    }
    m_DD_pv_start		= (FVF::L*)RCache.Vertex.Lock(MAX_VERT_COUNT,vs_L->vb_stride,m_DD_base);
    m_DD_pv				= m_DD_pv_start;
}
void CDrawUtilities::DD_DrawFace_flush(BOOL try_again)
{
    if (g_bEditorDX11) {
        u32 cnt = (u32)(m_DD_pv - m_DD_pv_start);
        if (cnt) HW11.DU_DrawPrim(m_DD_pv_start, cnt, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        if (try_again) m_DD_pv = m_DD_pv_start;
        return;
    }
    RCache.Vertex.Unlock((u32)(m_DD_pv-m_DD_pv_start),vs_L->vb_stride);
    if (m_DD_wire)		DU_DRAW_RS(D3DRS_FILLMODE,D3DFILL_WIREFRAME);
    DU_DRAW_DP			(D3DPT_TRIANGLELIST,vs_L,m_DD_base,u32(m_DD_pv-m_DD_pv_start)/3);
    if (m_DD_wire)		DU_DRAW_RS(D3DRS_FILLMODE,FILL_MODE);
    if (try_again){
	    m_DD_pv_start	= (FVF::L*)RCache.Vertex.Lock(MAX_VERT_COUNT,vs_L->vb_stride,m_DD_base);
        m_DD_pv			= m_DD_pv_start;
    }
}
void CDrawUtilities::DD_DrawFace_push(const Fvector& p0, const Fvector& p1, const Fvector& p2, u32 clr)
{
    m_DD_pv->set		(p0,clr); m_DD_pv++;
    m_DD_pv->set		(p1,clr); m_DD_pv++;
    m_DD_pv->set		(p2,clr); m_DD_pv++;
    if (m_DD_pv-m_DD_pv_start==MAX_VERT_COUNT)
        DD_DrawFace_flush	(TRUE); 
}
void CDrawUtilities::DD_DrawFace_end()
{
    DD_DrawFace_flush	(FALSE); 	
    m_DD_pv_start 		= 0;
}
//----------------------------------------------------

void CDrawUtilities::DrawCylinder(const Fmatrix& parent, const Fvector& center, const Fvector& dir, float height, float radius, u32 clr_s, u32 clr_w, BOOL bSolid, BOOL bWire)
{
    Fmatrix mScale;
    mScale.scale		(2.f*radius,2.f*radius,height);

    // build final rotation / translation
    Fvector             L_dir,L_up,L_right;
    L_dir.set           (dir);       		    L_dir.normalize			();
    L_up.set            (0,1,0);				if (_abs(L_up.dotproduct(L_dir))>.99f)  L_up.set(0,0,1);
    L_right.crossproduct(L_up,L_dir);           L_right.normalize       ();
    L_up.crossproduct   (L_dir,L_right);        L_up.normalize          ();

    Fmatrix         	mR;
    mR.i                = L_right;              mR._14          = 0;
    mR.j                = L_up;                 mR._24          = 0;
    mR.k                = L_dir;                mR._34          = 0;
    mR.c                = center;		  		mR._44          = 1;

    // final xform
    Fmatrix xf;			xf.mul (mR,mScale);
    xf.mulA_43			(parent);
    if (g_bEditorDX11) {
        if (bWire) {
            // unit cylinder: radius=0.5 in XY, z in [-0.5,+0.5]; draw 2 caps + 8 pillars
            const float da = PI_MUL_2 / LINE_DIVISION;
            FVF::L caps[2][LINE_DIVISION+1];
            for (int i=0; i<LINE_DIVISION; i++) {
                float ca = _cos(i*da), sa = _sin(i*da);
                Fvector tp; tp.set(0.5f*ca, 0.5f*sa,  0.5f); xf.transform_tiny(caps[0][i].p, tp); caps[0][i].color = clr_w;
                Fvector bp; bp.set(0.5f*ca, 0.5f*sa, -0.5f); xf.transform_tiny(caps[1][i].p, bp); caps[1][i].color = clr_w;
            }
            caps[0][LINE_DIVISION] = caps[0][0];
            caps[1][LINE_DIVISION] = caps[1][0];
            HW11.DU_DrawPrim(caps[0], LINE_DIVISION+1, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
            HW11.DU_DrawPrim(caps[1], LINE_DIVISION+1, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
            FVF::L pillars[8*2];
            for (int i=0; i<8; i++) {
                pillars[i*2+0] = caps[0][i * LINE_DIVISION/8];
                pillars[i*2+1] = caps[1][i * LINE_DIVISION/8];
            }
            HW11.DU_DrawPrim(pillars, 16, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        }
        return;
    }
    RCache.set_xform_world(xf);
	DrawIdentCylinder	(bSolid,bWire,clr_s,clr_w);
}
//----------------------------------------------------

void CDrawUtilities::DrawCone	(const Fmatrix& parent, const Fvector& apex, const Fvector& dir, float height, float radius, u32 clr_s, u32 clr_w, BOOL bSolid, BOOL bWire)
{
    Fmatrix mScale;
    mScale.scale		(2.f*radius,2.f*radius,height);

    // build final rotation / translation
    Fvector             L_dir,L_up,L_right;
    L_dir.set           (dir);       		    L_dir.normalize			();
    L_up.set            (0,1,0);				if (_abs(L_up.dotproduct(L_dir))>.99f)  L_up.set(0,0,1);
    L_right.crossproduct(L_up,L_dir);           L_right.normalize       ();
    L_up.crossproduct   (L_dir,L_right);        L_up.normalize          ();

    Fmatrix         	mR;
    mR.i                = L_right;              mR._14          = 0;
    mR.j                = L_up;                 mR._24          = 0;
    mR.k                = L_dir;                mR._34          = 0;
    mR.c                = apex;			  		mR._44          = 1;

    // final xform
    Fmatrix xf;			xf.mul (mR,mScale);
    xf.mulA_43			(parent);
    if (g_bEditorDX11) {
        if (bWire) {
            // du_cone: apex at z=0, base circle at z=1.0, radius=0.5
            const float da = PI_MUL_2 / LINE_DIVISION;
            FVF::L circle[LINE_DIVISION+1];
            for (int i=0; i<LINE_DIVISION; i++) {
                Fvector lp; lp.set(0.5f*_cos(i*da), 0.5f*_sin(i*da), 1.f);
                xf.transform_tiny(circle[i].p, lp); circle[i].color = clr_w;
            }
            circle[LINE_DIVISION] = circle[0];
            HW11.DU_DrawPrim(circle, LINE_DIVISION+1, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
            Fvector apex_local = {0.f, 0.f, 0.f};
            Fvector apex_w; xf.transform_tiny(apex_w, apex_local);
            FVF::L spokes[8*2];
            for (int i=0; i<8; i++) {
                spokes[i*2+0] = circle[i * LINE_DIVISION/8];
                spokes[i*2+1].set(apex_w, clr_w);
            }
            HW11.DU_DrawPrim(spokes, 16, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        }
        return;
    }
    RCache.set_xform_world(xf);
	DrawIdentCone		(bSolid,bWire,clr_s,clr_w);
}
//----------------------------------------------------

void CDrawUtilities::DrawPlane	(const Fvector& p, const Fvector& n, const Fvector2& scale, u32 clr_s, u32 clr_w, BOOL bCull, BOOL bSolid, BOOL bWire)
{
	if (n.square_magnitude()<EPS_S) return;
    // build final rotation / translation
    Fvector             L_dir,L_up=n,L_right;
    L_dir.set           (0,0,1);				if (_abs(L_up.dotproduct(L_dir))>.99f)  L_dir.set(1,0,0);
    L_right.crossproduct(L_up,L_dir);           L_right.normalize	();
    L_dir.crossproduct  (L_right,L_up);        	L_dir.normalize		();

    Fmatrix         	mR;
    mR.i                = L_right;              mR._14          = 0;
    mR.j                = L_up;                 mR._24          = 0;
    mR.k                = L_dir;                mR._34          = 0;
    mR.c                = p;			  		mR._44          = 1;

    if (g_bEditorDX11) {
        auto tx = [&](float lx, float ly, float lz, u32 c) -> FVF::L {
            FVF::L v; v.set(lx,0,lz,c); mR.transform_tiny(v.p); v.p.y += ly; return v;
        };
        if (bSolid) {
            FVF::L tv[6];
            tv[0]=tx(-scale.x,0,-scale.y,clr_s); tv[1]=tx(-scale.x,0,+scale.y,clr_s); tv[2]=tx(+scale.x,0,+scale.y,clr_s);
            tv[3]=tx(-scale.x,0,-scale.y,clr_s); tv[4]=tx(+scale.x,0,+scale.y,clr_s); tv[5]=tx(+scale.x,0,-scale.y,clr_s);
            HW11.DU_DrawPrim(tv, 6, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
        if (bWire) {
            FVF::L lv[5];
            lv[0]=tx(-scale.x,0,-scale.y,clr_w); lv[1]=tx(+scale.x,0,-scale.y,clr_w);
            lv[2]=tx(+scale.x,0,+scale.y,clr_w); lv[3]=tx(-scale.x,0,+scale.y,clr_w); lv[4]=lv[0];
            HW11.DU_DrawPrim(lv, 5, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        }
        return;
    }
	// fill VB
	_VertexStream*	Stream	= &RCache.Vertex;
	u32			vBase;

    if (bSolid){
	    DU_DRAW_SH(EDevice.m_SelectionShader);
        FVF::L*	pv	 = (FVF::L*)Stream->Lock(5,vs_L->vb_stride,vBase);
        pv->set		(-scale.x, 0, -scale.y, clr_s); mR.transform_tiny(pv->p); pv++;
        pv->set		(-scale.x, 0, +scale.y, clr_s); mR.transform_tiny(pv->p); pv++;
        pv->set		(+scale.x, 0, +scale.y, clr_s); mR.transform_tiny(pv->p); pv++;
        pv->set		(+scale.x, 0, -scale.y, clr_s); mR.transform_tiny(pv->p); pv++;
        pv->set		(*(pv-4));
        Stream->Unlock(5,vs_L->vb_stride);
        if (!bCull) DU_DRAW_RS(D3DRS_CULLMODE,D3DCULL_NONE);
        DU_DRAW_DP		(D3DPT_TRIANGLEFAN,vs_L,vBase,2);
        if (!bCull) DU_DRAW_RS(D3DRS_CULLMODE,D3DCULL_CCW);
    }

    if (bWire){
	    DU_DRAW_SH(EDevice.m_WireShader);
        FVF::L*	pv	 = (FVF::L*)Stream->Lock(5,vs_L->vb_stride,vBase);
        pv->set		(-scale.x, 0, -scale.y, clr_w); mR.transform_tiny(pv->p); pv++;
        pv->set		(+scale.x, 0, -scale.y, clr_w); mR.transform_tiny(pv->p); pv++;
        pv->set		(+scale.x, 0, +scale.y, clr_w); mR.transform_tiny(pv->p); pv++;
        pv->set		(-scale.x, 0, +scale.y, clr_w); mR.transform_tiny(pv->p); pv++;
        pv->set		(*(pv-4));
        Stream->Unlock(5,vs_L->vb_stride);
	    DU_DRAW_DP	(D3DPT_LINESTRIP,vs_L,vBase,4);
    }
}
//----------------------------------------------------

void CDrawUtilities::DrawPlane  (const Fvector& center, const Fvector2& scale, const Fvector& rotate, u32 clr_s, u32 clr_w, BOOL bCull, BOOL bSolid, BOOL bWire)
{
    Fmatrix M;
    M.setHPB		(rotate.y,rotate.x,rotate.z);
    M.translate_over(center);
    if (g_bEditorDX11) {
        auto tx = [&](float lx, float lz, u32 c) -> FVF::L {
            FVF::L v; v.set(lx,0,lz,c); M.transform_tiny(v.p); return v;
        };
        if (bSolid) {
            FVF::L tv[6];
            tv[0]=tx(-scale.x,-scale.y,clr_s); tv[1]=tx(-scale.x,+scale.y,clr_s); tv[2]=tx(+scale.x,+scale.y,clr_s);
            tv[3]=tx(-scale.x,-scale.y,clr_s); tv[4]=tx(+scale.x,+scale.y,clr_s); tv[5]=tx(+scale.x,-scale.y,clr_s);
            HW11.DU_DrawPrim(tv, 6, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
        if (bWire) {
            FVF::L lv[5];
            lv[0]=tx(-scale.x,-scale.y,clr_w); lv[1]=tx(+scale.x,-scale.y,clr_w);
            lv[2]=tx(+scale.x,+scale.y,clr_w); lv[3]=tx(-scale.x,+scale.y,clr_w); lv[4]=lv[0];
            HW11.DU_DrawPrim(lv, 5, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        }
        return;
    }
	// fill VB
	_VertexStream*	Stream	= &RCache.Vertex;
	u32			vBase;

    if (bSolid){
	    DU_DRAW_SH(EDevice.m_SelectionShader);
        FVF::L*	pv	 = (FVF::L*)Stream->Lock(5,vs_L->vb_stride,vBase);
        pv->set		(-scale.x, 0, -scale.y, clr_s); M.transform_tiny(pv->p); pv++;
        pv->set		(-scale.x, 0, +scale.y, clr_s); M.transform_tiny(pv->p); pv++;
        pv->set		(+scale.x, 0, +scale.y, clr_s); M.transform_tiny(pv->p); pv++;
        pv->set		(+scale.x, 0, -scale.y, clr_s); M.transform_tiny(pv->p); pv++;
        pv->set		(*(pv-4));
        Stream->Unlock(5,vs_L->vb_stride);
        if (!bCull) DU_DRAW_RS(D3DRS_CULLMODE,D3DCULL_NONE);
        DU_DRAW_DP		(D3DPT_TRIANGLEFAN,vs_L,vBase,2);
        if (!bCull) DU_DRAW_RS(D3DRS_CULLMODE,D3DCULL_CCW);
    }

    if (bWire){
	    DU_DRAW_SH(EDevice.m_WireShader);
        FVF::L*	pv	 = (FVF::L*)Stream->Lock(5,vs_L->vb_stride,vBase);
        pv->set		(-scale.x, 0, -scale.y, clr_w); M.transform_tiny(pv->p); pv++;
        pv->set		(+scale.x, 0, -scale.y, clr_w); M.transform_tiny(pv->p); pv++;
        pv->set		(+scale.x, 0, +scale.y, clr_w); M.transform_tiny(pv->p); pv++;
        pv->set		(-scale.x, 0, +scale.y, clr_w); M.transform_tiny(pv->p); pv++;
        pv->set		(*(pv-4));
        Stream->Unlock(5,vs_L->vb_stride);
	    DU_DRAW_DP	(D3DPT_LINESTRIP,vs_L,vBase,4);
    }
}
//----------------------------------------------------

void CDrawUtilities::DrawRectangle(const Fvector& o, const Fvector& u, const Fvector& v, u32 clr_s, u32 clr_w, BOOL bSolid, BOOL bWire)
{
    if (g_bEditorDX11) {
        if (bSolid) {
            FVF::L tv[6];
            tv[0].set(o,clr_s);                                       tv[1].set(o.x+u.x+v.x,o.y+u.y+v.y,o.z+u.z+v.z,clr_s); tv[2].set(o.x+v.x,o.y+v.y,o.z+v.z,clr_s);
            tv[3].set(o,clr_s);                                       tv[4].set(o.x+u.x,o.y+u.y,o.z+u.z,clr_s);              tv[5]=tv[1];
            HW11.DU_DrawPrim(tv, 6, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
        if (bWire) {
            FVF::L lv[5];
            lv[0].set(o,clr_w); lv[1].set(o.x+u.x,o.y+u.y,o.z+u.z,clr_w);
            lv[2].set(o.x+u.x+v.x,o.y+u.y+v.y,o.z+u.z+v.z,clr_w); lv[3].set(o.x+v.x,o.y+v.y,o.z+v.z,clr_w); lv[4]=lv[0];
            HW11.DU_DrawPrim(lv, 5, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        }
        return;
    }
	_VertexStream*	Stream	= &RCache.Vertex;

    u32				vBase;
    if (bSolid)
    {
	    DU_DRAW_SH(EDevice.m_SelectionShader);
        FVF::L*	pv	 	= (FVF::L*)Stream->Lock(6,vs_L->vb_stride,vBase);
        pv->set			(o.x,			o.y,		o.z,		clr_s); pv++;
        pv->set			(o.x+u.x+v.x,	o.y+u.y+v.y,o.z+u.z+v.z,clr_s); pv++;
        pv->set			(o.x+v.x,		o.y+v.y,	o.z+v.z,	clr_s); pv++;
        pv->set			(o.x,			o.y,		o.z,		clr_s); pv++;
        pv->set			(o.x+u.x,		o.y+u.y,	o.z+u.z,	clr_s); pv++;
        pv->set			(o.x+u.x+v.x,	o.y+u.y+v.y,o.z+u.z+v.z,clr_s); pv++;
        Stream->Unlock	(6,vs_L->vb_stride);
	    DU_DRAW_DP		(D3DPT_TRIANGLELIST,vs_L,vBase,2);
    }
    if (bWire)
    {
	    DU_DRAW_SH(EDevice.m_WireShader);
        FVF::L*	pv	 	= (FVF::L*)Stream->Lock(5,vs_L->vb_stride,vBase);
        pv->set			(o.x,			o.y,		o.z,		clr_w); pv++;
        pv->set			(o.x+u.x,		o.y+u.y,	o.z+u.z,	clr_w); pv++;
        pv->set			(o.x+u.x+v.x,	o.y+u.y+v.y,o.z+u.z+v.z,clr_w); pv++;
        pv->set			(o.x+v.x,		o.y+v.y,	o.z+v.z,	clr_w); pv++;
        pv->set			(o.x,			o.y,		o.z,		clr_w); pv++;
        Stream->Unlock	(5,vs_L->vb_stride);
	    DU_DRAW_DP		(D3DPT_LINESTRIP,vs_L,vBase,4);
    }
}
//----------------------------------------------------

void CDrawUtilities::DrawCross(const Fvector& p, float szx1, float szy1, float szz1, float szx2, float szy2, float szz2, u32 clr, BOOL bRot45)
{
    if (g_bEditorDX11) {
        FVF::L pvArr[12];
        pvArr[0].set(p.x+szx2,p.y,p.z,clr); pvArr[1].set(p.x-szx1,p.y,p.z,clr);
        pvArr[2].set(p.x,p.y+szy2,p.z,clr); pvArr[3].set(p.x,p.y-szy1,p.z,clr);
        pvArr[4].set(p.x,p.y,p.z+szz2,clr); pvArr[5].set(p.x,p.y,p.z-szz1,clr);
        if (bRot45) {
            Fmatrix M; M.setHPB(PI_DIV_4,PI_DIV_4,PI_DIV_4);
            for (int i=0; i<6; i++) {
                pvArr[6+i].p.sub(pvArr[i].p, p);
                M.transform_dir(pvArr[6+i].p);
                pvArr[6+i].p.add(p);
                pvArr[6+i].color = clr;
            }
        }
        HW11.DU_DrawPrim(pvArr, bRot45?12:6, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        return;
    }
	_VertexStream*	Stream	= &RCache.Vertex;
	// actual rendering
	u32			vBase;
	FVF::L*	pv	 	= (FVF::L*)Stream->Lock(bRot45?12:6,vs_L->vb_stride,vBase);
    pv->set(p.x+szx2,p.y,p.z,clr); pv++;
    pv->set(p.x-szx1,p.y,p.z,clr); pv++;
    pv->set(p.x,p.y+szy2,p.z,clr); pv++;
    pv->set(p.x,p.y-szy1,p.z,clr); pv++;
    pv->set(p.x,p.y,p.z+szz2,clr); pv++;
    pv->set(p.x,p.y,p.z-szz1,clr); pv++;
    if (bRot45){
    	Fmatrix M;
        M.setHPB(PI_DIV_4,PI_DIV_4,PI_DIV_4);
	    for(int i=0;i<6;i++,pv++){
        	pv->p.sub((pv-6)->p,p);
        	M.transform_dir(pv->p);
        	pv->p.add(p);
            pv->color = clr;
        }
    }
	// unlock VB and Render it as triangle list
	Stream->Unlock(bRot45?12:6,vs_L->vb_stride);
    DU_DRAW_DP			(D3DPT_LINELIST,vs_L,vBase,bRot45?6:3);
}

void CDrawUtilities::DrawPivot(const Fvector& pos, float sz){
	DU_DRAW_SH(EDevice.m_WireShader);
    if (g_bEditorDX11) {
        // Disable depth test: cross arms are coplanar with the grid (Y=0) and below it (Y-),
        // so depth LESS_EQUAL would clip or z-fight them. Draw always-on-top.
        bool saved = HW11.States.depth_enable;
        HW11.States.depth_enable = false;
        HW11.States.ds_dirty = true;
        DrawCross(pos, sz, sz, sz, sz, sz, sz, 0xFF7FFF7F);
        HW11.States.depth_enable = saved;
        HW11.States.ds_dirty = true;
        HW11.FlushStates(); // push restored state to hardware immediately;
                            // mesh draws after this (RenderInstBatchesDX11) skip FlushStates
    } else {
        DrawCross(pos, sz, sz, sz, sz, sz, sz, 0xFF7FFF7F);
    }
}

void CDrawUtilities::DrawAxis(const Fmatrix& T)
{
    // DX11: no early-return — CEditableObject::Render is ported to HW11, so the "editor\axis" 3D
    // model below renders in both APIs. Its arrows (surfaces axis_x/y/z) are coloured by solid
    // textures prop\prop_color_r/g/b, which the DX11 mesh path (DrawMeshTex → surface texture)
    // draws correctly, so the RGB comes through.
/*
	_VertexStream*	Stream	= &RCache.Vertex;
    Fvector p[6];
    u32 	c[6];

    // colors
    c[0]=c[2]=c[4]=0x00222222; c[1]=0x00FF0000; c[3]=0x0000FF00; c[5]=0x000000FF;

    // position
  	p[0].mad(T.c,T.k,0.25f);
    p[1].set(p[0]); p[1].x+=.015f;
    p[2].set(p[0]);
    p[3].set(p[0]); p[3].y+=.015f;
    p[4].set(p[0]);
    p[5].set(p[0]); p[5].z+=.015f;

    u32 vBase;
	FVF::TL* pv	= (FVF::TL*)Stream->Lock(6,vs_TL->vb_stride,vBase);
    // transform to screen
    float dx=-float(EDevice.dwWidth)/2.2f;
    float dy=float(EDevice.dwHeight)/2.25f;

    for (int i=0; i<6; i++,pv++)
    {
	    pv->color 		= c[i]; 
        pv->transform	(p[i],EDevice.mFullTransform);
	    pv->p.set((float)iFloor(_x2real(pv->p.x)+dx),(float)iFloor(_y2real(pv->p.y)+dy),0,1);
        p[i].set(pv->p.x,pv->p.y,0);
    }

	// unlock VB and Render it as triangle list
	Stream->Unlock(6,vs_TL->vb_stride);
	DU_DRAW_RS(D3DRS_SHADEMODE,D3DSHADE_GOURAUD);
	DU_DRAW_SH(EDevice.m_WireShader);
    DU_DRAW_DP(D3DPT_LINELIST,vs_TL,vBase,3);
	DU_DRAW_RS(D3DRS_SHADEMODE,SHADE_MODE);

    m_Font->SetColor(0xFF909090);
    m_Font->Out(p[1].x,p[1].y,"x");
    m_Font->Out(p[3].x,p[3].y,"y");
    m_Font->Out(p[5].x,p[5].y,"z");
    m_Font->SetColor(0xFF000000);
    m_Font->Out(p[1].x-1,p[1].y-1,"x");
    m_Font->Out(p[3].x-1,p[3].y-1,"y");
    m_Font->Out(p[5].x-1,p[5].y-1,"z");
*/
    if(!m_axis_object)
    {
        m_axis_object = Lib.CreateEditObject("editor\\axis");
    }
    
    Fmatrix	M 				= Fidentity;
    Fmatrix	S;
    S.scale					(0.04f,0.04f,0.04f);
    M.mulB_44				(S);

    Fvector start, dir;
    Ivector2 pt;

    static int _wh			= 50;
    static float _kl		= 1.0f;

	pt.x  = _wh;
	pt.y  = iFloor(UI->GetRealHeight()-_wh);

    EDevice.m_Camera.MouseRayFromPoint(M.c, dir, pt);
    M.c.mad(dir, _kl);
    if (g_bEditorDX11)
        // Surface shaders (and thus _Priority()) aren't built in the DX11 editor, so the axis
        // surfaces fall back to priority 1 — Render(M,2,false) would skip them. RenderSingle
        // iterates every priority/side, so the model draws regardless.
        m_axis_object->RenderSingle(M);
    else
        m_axis_object->Render	(M, 2, false);
}

void CDrawUtilities::DrawObjectAxis(const Fmatrix& T, float sz, BOOL sel)
{
	VERIFY( EDevice.b_is_Ready );
    Fvector c,r,n,d;
	float w	= T.c.x*EDevice.mFullTransform._14 + T.c.y*EDevice.mFullTransform._24 + T.c.z*EDevice.mFullTransform._34 + EDevice.mFullTransform._44;
    if (w<0) return; // culling

	float s = w*sz;

	EDevice.mFullTransform.transform(c,T.c);
    r.mul(T.i,s); r.add(T.c); EDevice.mFullTransform.transform(r);
    n.mul(T.j,s); n.add(T.c); EDevice.mFullTransform.transform(n);
    d.mul(T.k,s); d.add(T.c); EDevice.mFullTransform.transform(d);

    if (g_bEditorDX11) {
        // c/r/n/d are in NDC after transform; y=+1 is top, matches DX11
        FVF::L lv[6];
        lv[0].set(c.x,c.y,0,0xFF222222);          lv[1].set(d.x,d.y,0,sel?0xFF0000FF:0xFF000080);
        lv[2].set(c.x,c.y,0,0xFF222222);          lv[3].set(r.x,r.y,0,sel?0xFFFF0000:0xFF800000);
        lv[4].set(c.x,c.y,0,0xFF222222);          lv[5].set(n.x,n.y,0,sel?0xFF00FF00:0xFF008000);
        HW11.DU_DrawPrim2D(lv, 6, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        return;
    }

	c.x = (float)iFloor(_x2real(c.x)); c.y = (float)iFloor(_y2real(-c.y));
    r.x = (float)iFloor(_x2real(r.x)); r.y = (float)iFloor(_y2real(-r.y));
    n.x = (float)iFloor(_x2real(n.x)); n.y = (float)iFloor(_y2real(-n.y));
    d.x = (float)iFloor(_x2real(d.x)); d.y = (float)iFloor(_y2real(-d.y));

	_VertexStream*	Stream	= &RCache.Vertex;
    u32 vBase;
	FVF::TL* pv	= (FVF::TL*)Stream->Lock(6,vs_TL->vb_stride,vBase);
	pv->set	(c.x,	c.y,	0,	1, 0xFF222222, 					0,0); 	pv++;
	pv->set	(d.x,	d.y,	0,	1, sel?0xFF0000FF:0xFF000080, 	0,0); 	pv++;
	pv->set	(c.x,	c.y,	0,	1, 0xFF222222, 					0,0); 	pv++;
	pv->set	(r.x,	r.y,	0,	1, sel?0xFFFF0000:0xFF800000, 	0,0); 	pv++;
	pv->set	(c.x,	c.y,	0,	1, 0xFF222222, 					0,0); 	pv++;
	pv->set	(n.x,	n.y,	0,	1, sel?0xFF00FF00:0xFF008000, 	0,0);
	Stream->Unlock(6,vs_TL->vb_stride);

	// Render it as line list
	DU_DRAW_RS	(D3DRS_SHADEMODE,D3DSHADE_GOURAUD);
	DU_DRAW_SH	(EDevice.m_WireShader);
    DU_DRAW_DP	(D3DPT_LINELIST,vs_TL,vBase,3);
	DU_DRAW_RS	(D3DRS_SHADEMODE,SHADE_MODE);

    m_Font->SetColor(sel?0xFF000000:0xFF909090);
    m_Font->Out(r.x,r.y,"x");
    m_Font->Out(n.x,n.y,"y");
    m_Font->Out(d.x,d.y,"z");
    m_Font->SetColor(sel?0xFFFFFFFF:0xFF000000);
    m_Font->Out(r.x-1,r.y-1,"x");
    m_Font->Out(n.x-1,n.y-1,"y");
    m_Font->Out(d.x-1,d.y-1,"z");
}

void CDrawUtilities::DrawGrid()
{
	VERIFY( EDevice.b_is_Ready );
    if (g_bEditorDX11) {
        if (!m_GridPoints.empty())
            HW11.DU_DrawPrim(m_GridPoints.data(), (u32)m_GridPoints.size(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        return;
    }
	_VertexStream*	Stream	= &RCache.Vertex;
    u32 vBase;
	// fill VB
	FVF::L*	pv	= (FVF::L*)Stream->Lock(m_GridPoints.size(),vs_L->vb_stride,vBase);
    for (FLvertexIt v_it=m_GridPoints.begin(); v_it!=m_GridPoints.end(); v_it++,pv++) pv->set(*v_it);
	Stream->Unlock(m_GridPoints.size(),vs_L->vb_stride);
	// Render it as triangle list
    Fmatrix ddd;
    ddd.identity();
    RCache.set_xform_world(ddd);
	DU_DRAW_SH(EDevice.m_WireShader);
    DU_DRAW_DP(D3DPT_LINELIST,vs_L,vBase,m_GridPoints.size()/2);
}

void CDrawUtilities::DrawSelectionRect(const Ivector2& m_SelStart, const Ivector2& m_SelEnd){
	VERIFY( EDevice.b_is_Ready );
    if (g_bEditorDX11) {
        float W = (float)HW11.BackBufferW, H = (float)HW11.BackBufferH;
        auto px2nx = [&](float x) { return 2.f * x / W - 1.f; };
        auto py2ny = [&](float y) { return 1.f - 2.f * y / H; };

        // Normalise to min/max so the TRIANGLESTRIP always has CW winding in NDC.
        // Without this, dragging TR→BL or BL→TR produces CCW triangles that DX11 culls.
        float sx0 = (float)std::min(m_SelStart.x, m_SelEnd.x);
        float sx1 = (float)std::max(m_SelStart.x, m_SelEnd.x);
        float sy0 = (float)std::min(m_SelStart.y, m_SelEnd.y);  // top in screen (smallest y)
        float sy1 = (float)std::max(m_SelStart.y, m_SelEnd.y);  // bottom in screen

        // Fill: semi-transparent green (ARGB 0x5000FF00 = alpha~31%, green)
        // Vertex order: TL, TR, BL, BR → always CW in NDC (front-facing)
        u32 fill_col = D3DCOLOR_ARGB(80, 0, 255, 0);
        FVF::L fill[4];
        fill[0].set(px2nx(sx0), py2ny(sy0), 0.f, fill_col);  // TL
        fill[1].set(px2nx(sx1), py2ny(sy0), 0.f, fill_col);  // TR
        fill[2].set(px2nx(sx0), py2ny(sy1), 0.f, fill_col);  // BL
        fill[3].set(px2nx(sx1), py2ny(sy1), 0.f, fill_col);  // BR
        float bf[4] = {};
        HW11.pContext->OMSetBlendState(EditorShaders11.bs_alpha, bf, 0xFFFFFFFF);
        HW11.DU_DrawPrim2D(fill, 4, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        // Outline: white LINESTRIP (5 verts closing the loop, winding irrelevant for lines)
        u32 col = 0xFFFFFFFF;
        FVF::L verts[5];
        verts[0].set(px2nx(sx0), py2ny(sy0), 0.f, col);  // TL
        verts[1].set(px2nx(sx1), py2ny(sy0), 0.f, col);  // TR
        verts[2].set(px2nx(sx1), py2ny(sy1), 0.f, col);  // BR
        verts[3].set(px2nx(sx0), py2ny(sy1), 0.f, col);  // BL
        verts[4] = verts[0];
        HW11.pContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        HW11.DU_DrawPrim2D(verts, 5, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        HW11.States.bs_dirty = true;
        return;
    }
	// fill VB
	_VertexStream*	Stream	= &RCache.Vertex;
    u32 vBase;
	FVF::TL* pv	= (FVF::TL*)Stream->Lock(4,vs_TL->vb_stride,vBase);
    pv->set(m_SelStart.x*SCREEN_QUALITY, m_SelStart.y*SCREEN_QUALITY, m_SelectionRect,0.f,0.f); pv++;
    pv->set(m_SelStart.x*SCREEN_QUALITY, m_SelEnd.y*SCREEN_QUALITY,   m_SelectionRect,0.f,0.f); pv++;
    pv->set(m_SelEnd.x*SCREEN_QUALITY,   m_SelEnd.y*SCREEN_QUALITY,   m_SelectionRect,0.f,0.f); pv++;
    pv->set(m_SelEnd.x*SCREEN_QUALITY,   m_SelStart.y*SCREEN_QUALITY, m_SelectionRect,0.f,0.f); pv++;
	Stream->Unlock(4,vs_TL->vb_stride);
	// Render it as triangle list
    DU_DRAW_RS(D3DRS_CULLMODE,D3DCULL_NONE);
	DU_DRAW_SH(EDevice.m_SelectionShader);
    DU_DRAW_DP(D3DPT_TRIANGLEFAN,vs_TL,vBase,2);
    DU_DRAW_RS(D3DRS_CULLMODE,D3DCULL_CCW);
}

void CDrawUtilities::DrawPrimitiveL	(D3DPRIMITIVETYPE pt, u32 pc, Fvector* vertices, int vc, u32 color, BOOL bCull, BOOL bCycle)
{
    if (g_bEditorDX11) {
        u32 dwNeed = bCycle ? vc + 1 : vc;
        xr_vector<FVF::L> tmp(dwNeed);
        for (int k = 0; k < vc; k++) tmp[k].set(vertices[k], color);
        if (bCycle) tmp[vc].set(vertices[0], color);
        D3D11_PRIMITIVE_TOPOLOGY topo;
        switch (pt) {
        case D3DPT_LINESTRIP:       topo = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;    break;
        case D3DPT_TRIANGLELIST:    topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
        case D3DPT_TRIANGLESTRIP:   topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
        case D3DPT_TRIANGLEFAN:     topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break; // approximate
        default:                    topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;      break;
        }
        HW11.DU_DrawPrim(tmp.data(), dwNeed, topo);
        return;
    }
	// fill VB
	_VertexStream*	Stream	= &RCache.Vertex;
	u32			vBase, dwNeed=(bCycle)?vc+1:vc;
	FVF::L*	pv	= (FVF::L*)Stream->Lock(dwNeed,vs_L->vb_stride,vBase);
    for(int k=0; k<vc; k++,pv++)
    	pv->set		(vertices[k],color);
    if (bCycle)		pv->set(*(pv-vc));
	Stream->Unlock(dwNeed,vs_L->vb_stride);

    if (!bCull) DU_DRAW_RS(D3DRS_CULLMODE,D3DCULL_NONE);
    DU_DRAW_DP		(pt,vs_L,vBase,pc);
    if (!bCull) DU_DRAW_RS(D3DRS_CULLMODE,D3DCULL_CCW);
}

void CDrawUtilities::DrawIndexedPrimitive(	int pt,
											u32 pc,
                                            const Fvector& pos,
                                            const Fvector* vb,
                                            const u32& vb_size,
                                            const u32* ib,
                                            const u32& ib_size,
                                            const u32& clr_argb,
                                            float scale)
{
    if (g_bEditorDX11) {
        // expand indexed geometry into flat list
        xr_vector<FVF::L> tmp(ib_size);
        for (u32 k=0; k<ib_size; ++k)
            tmp[k].set(Fvector().add(pos, Fvector().mul(vb[ib[k]], scale)), clr_argb);
        D3D11_PRIMITIVE_TOPOLOGY topo = (pt == (int)D3DPT_TRIANGLELIST)
            ? D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
            : D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        HW11.DU_DrawPrim(tmp.data(), ib_size, topo);
        return;
    }
	_VertexStream* Stream	= &RCache.Vertex;
	_IndexStream*	StreamI	= &RCache.Index;

	u32 vBase, iBase;
    WORD* i;

	FVF::L* pv				= (FVF::L*)Stream->Lock(vb_size, vs_L->vb_stride, vBase);
    for(int k=0; k<vb_size; ++k,++pv)
    	pv->set		(Fvector().add(pos, Fvector().mul(vb[k],scale)), clr_argb);

	Stream->Unlock(vb_size, vs_L->vb_stride);

    i 				= StreamI->Lock(ib_size,iBase);
    for (int k=0; k<ib_size; ++k,++i)
    	*i= ib[k];

    StreamI->Unlock(ib_size);

    EDevice.SetShader	(EDevice.m_SelectionShader);
	// and Render it as triangle list
	DU_DRAW_DIP	((D3DPRIMITIVETYPE)pt, vs_L, vBase, 0, vb_size, iBase, pc);
}

void CDrawUtilities::DrawPrimitiveTL(D3DPRIMITIVETYPE pt, u32 pc, FVF::TL* vertices, int vc, BOOL bCull, BOOL bCycle)
{
    if (g_bEditorDX11) {
        // TL vertices have pixel-space coords; convert to NDC for the 2D prim shader
        float W = (float)HW11.BackBufferW, H = (float)HW11.BackBufferH;
        u32 dwNeed = bCycle ? vc + 1 : vc;
        xr_vector<FVF::L> tmp(dwNeed);
        for (int k = 0; k < vc; k++) {
            float ndx = 2.f * vertices[k].p.x / W - 1.f;
            float ndy = 1.f - 2.f * vertices[k].p.y / H;
            tmp[k].set(ndx, ndy, 0.f, vertices[k].color);
        }
        if (bCycle) tmp[vc] = tmp[0];
        D3D11_PRIMITIVE_TOPOLOGY topo;
        switch (pt) {
        case D3DPT_LINESTRIP:     topo = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;     break;
        case D3DPT_POINTLIST:     topo = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;     break;
        case D3DPT_TRIANGLESTRIP: topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
        case D3DPT_TRIANGLELIST:  topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;  break;
        default:                  topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;       break;
        }
        HW11.DU_DrawPrim2D(tmp.data(), dwNeed, topo);
        return;
    }
	// fill VB
	_VertexStream*	Stream	= &RCache.Vertex;
	u32			vBase, dwNeed=(bCycle)?vc+1:vc;
	FVF::TL* pv		= (FVF::TL*)Stream->Lock(dwNeed,vs_TL->vb_stride,vBase);
    for(int k=0; k<vc; k++,pv++)
    	pv->set		(vertices[k]);
    if (bCycle)		pv->set(*(pv-vc));
	Stream->Unlock(dwNeed,vs_TL->vb_stride);

    if (!bCull) 	DU_DRAW_RS(D3DRS_CULLMODE,D3DCULL_NONE);
    DU_DRAW_DP		(pt,vs_TL,vBase,pc);
    if (!bCull) 	DU_DRAW_RS(D3DRS_CULLMODE,D3DCULL_CCW);
}

void CDrawUtilities::DrawPrimitiveLIT(D3DPRIMITIVETYPE pt, u32 pc, FVF::LIT* vertices, int vc, BOOL bCull, BOOL bCycle)
{
    if (g_bEditorDX11) return; // LIT (textured) primitives not implemented for DX11
	// fill VB
	_VertexStream*	Stream	= &RCache.Vertex;
	u32			vBase, dwNeed=(bCycle)?vc+1:vc;
	FVF::LIT* 	pv	= (FVF::LIT*)Stream->Lock(dwNeed,vs_LIT->vb_stride,vBase);
    for(int k=0; k<vc; k++,pv++)
    	pv->set		(vertices[k]);
    if (bCycle)		pv->set(*(pv-vc));
	Stream->Unlock(dwNeed,vs_LIT->vb_stride);

    if (!bCull) DU_DRAW_RS(D3DRS_CULLMODE,D3DCULL_NONE);
    DU_DRAW_DP		(pt,vs_LIT,vBase,pc);
    if (!bCull) DU_DRAW_RS(D3DRS_CULLMODE,D3DCULL_CCW);
}

void CDrawUtilities::DrawLink(const Fvector& p0, const Fvector& p1, float sz, u32 clr)
{
	DrawLine(p1,p0,clr);
    Fvector pp[2],D,R,N={0,1,0};
    D.sub(p1,p0); D.normalize();
    R.crossproduct(N,D); R.mul(0.5f); D.mul(2.0f); N.mul(0.5f);
    // LR
	pp[0].add(R,D); pp[0].mul(sz*-0.5f);	pp[0].add(p1);
	R.invert();
	pp[1].add(R,D); pp[1].mul(sz*-0.5f);	pp[1].add(p1);
	DrawLine(p1,pp[0],clr);
 	DrawLine(p1,pp[1],clr);
    // UB
	pp[0].add(N,D); pp[0].mul(sz*-0.5f);	pp[0].add(p1);
    N.invert();
	pp[1].add(N,D); pp[1].mul(sz*-0.5f);	pp[1].add(p1);
    DrawLine(p1,pp[0],clr);
    DrawLine(p1,pp[1],clr);
}

void CDrawUtilities::DrawJoint(const Fvector& p, float radius, u32 clr)
{
  //	RCache.set_xform_world	(Fidentity);
	DrawLineSphere(p,radius,clr,false);
}

void CDrawUtilities::OnRender()
{
    if (m_Font) m_Font->OnRender();
}

void CDrawUtilities::OutText(const Fvector& pos, LPCSTR text, u32 color, u32 shadow_color)
{
    if (g_bEditorDX11) {
        // DX11: project to screen and queue into EditorFont11 (flushed once per frame
        // in CEditorRenderDevice::End). m_Font (DX9) is not available here.
        float w = pos.x*EDevice.mFullTransform._14 + pos.y*EDevice.mFullTransform._24
                + pos.z*EDevice.mFullTransform._34 + EDevice.mFullTransform._44;
        if (w >= 0.f) {
            Fvector p; EDevice.mFullTransform.transform(p, pos);
            float sx = (float)iFloor(_x2real(p.x));
            float sy = (float)iFloor(_y2real(-p.y));
            EditorFont11.SetColor(shadow_color);
            EditorFont11.OutSet (sx, sy);
            EditorFont11.OutNext("%s", text);
            EditorFont11.SetColor(color);
            EditorFont11.OutSet (sx-1.f, sy-1.f);
            EditorFont11.OutNext("%s", text);
        }
        return;
    }
    if (!m_Font) return;
	Fvector p;
	float w	= pos.x*EDevice.mFullTransform._14 + pos.y*EDevice.mFullTransform._24 + pos.z*EDevice.mFullTransform._34 + EDevice.mFullTransform._44;
	if (w>=0){
		EDevice.mFullTransform.transform(p,pos);
		p.x = (float)iFloor(_x2real(p.x)); p.y = (float)iFloor(_y2real(-p.y));

		m_Font->SetColor(shadow_color);
		m_Font->Out(p.x,p.y,(LPSTR)text);
		m_Font->SetColor(color);
		m_Font->Out(p.x-1,p.y-1,(LPSTR)text);
	}
}
