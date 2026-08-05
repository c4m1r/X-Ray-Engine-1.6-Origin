//---------------------------------------------------------------------------
#include "stdafx.h"
#pragma hdrstop

#include "EShape.h"
#include "../../ecore/editor/D3DUtils.h"
#include "du_box.h"
#include "Scene.h"

#define SHAPE_COLOR_TRANSP		0x3C808080
#define SHAPE_COLOR_EDGE		0xFF202020

//---------------------------------------------------------------------------

#define SHAPE_CURRENT_VERSION 	0x0002
//---------------------------------------------------------------------------
#define SHAPE_CHUNK_VERSION 	0x0000
#define SHAPE_CHUNK_SHAPES 		0x0001
#define SHAPE_CHUNK_DATA 		0x0002
//---------------------------------------------------------------------------

xr_token shape_type_tok[]={
	{ "common",	eShapeCommon		},
	{ "level bound",	eShapeLevelBound},
	{ 0,				0	}
};

CEditShape::CEditShape(LPVOID data, LPCSTR name):CCustomObject(data,name)
{
	Construct(data);
}

CEditShape::~CEditShape()
{
}

void CEditShape::Construct(LPVOID data)
{
	ClassID				= OBJCLASS_SHAPE;
    m_DrawTranspColor	= SHAPE_COLOR_TRANSP;
    m_DrawEdgeColor		= SHAPE_COLOR_EDGE;
    m_shape_type		= eShapeCommon;
	m_Box.invalidate();
}

void CEditShape::OnUpdateTransform()
{
	inherited::OnUpdateTransform();
}

void CEditShape::ComputeBounds()
{
	m_Box.invalidate	();

	for (ShapeIt it=shapes.begin(); it!=shapes.end(); it++){
		switch (it->type){
		case cfSphere:{
            Fsphere&	T		= it->data.sphere;
            Fvector		P;
            P.set		(T.P);	P.sub(T.R);	m_Box.modify(P);
            P.set		(T.P);	P.add(T.R);	m_Box.modify(P);
		}break;
		case cfBox:{
            Fvector		P;
            Fmatrix&	T		= it->data.box;

            // Build points
            Fvector p;
            for (int i=0; i<DU_BOX_NUMVERTEX; i++){
                T.transform_tiny	(P,du_box_vertices[i]);
                m_Box.modify		(P);
            }
		}break;
		}
	}
	m_Box.getsphere(m_Sphere.P,m_Sphere.R);
}

void CEditShape::SetScale(const Fvector& val)
{
	if (shapes.size()==1){
		switch (shapes[0].type){
		case cfSphere:{
        	FScale.set(val.x,val.x,val.x);
        }break;
		case cfBox:		FScale.set(val.x,val.y,val.z);	break;
        default: THROW;
		}
    }else{
		FScale.set(val.x,val.x,val.x);
    }
	ComputeBounds	();
    UpdateTransform	();
}

void CEditShape::ApplyScale()
{
	for (ShapeIt it=shapes.begin(); it!=shapes.end(); it++){
		switch (it->type){
		case cfSphere:{
            Fsphere&	T	= it->data.sphere;
            FTransformS.transform_tiny(T.P);
            T.R				*= PScale.x;
		}break;
		case cfBox:{
            Fmatrix& B		= it->data.box;
            B.mulA_43		(FTransformS);
		}break;
        }
    }
    FScale.set		(1.f,1.f,1.f);
    UpdateTransform	(true);

    ComputeBounds	();
}

void CEditShape::add_sphere(const Fsphere& S)
{
	shapes.push_back(shape_def());
	shapes.back().type	= cfSphere;
	shapes.back().data.sphere.set(S);

	ComputeBounds();
}

void CEditShape::add_box(const Fmatrix& B)
{
	shapes.push_back(shape_def());
	shapes.back().type	= cfBox;
	shapes.back().data.box.set(B);

	ComputeBounds();
}

void CEditShape::Attach(CEditShape* from)
{
	ApplyScale				();
	// transfer data
    from->ApplyScale		();
	Fmatrix M 				= from->_Transform();
    M.mulA_43				(_ITransform());
	for (ShapeIt it=from->shapes.begin(); it!=from->shapes.end(); it++){
		switch (it->type){
		case cfSphere:{
            Fsphere& T		= it->data.sphere;
            M.transform_tiny(T.P);
            add_sphere		(T);
		}break;
		case cfBox:{
            Fmatrix B		= it->data.box;
            B.mulA_43		(M);
            add_box			(B);
		}break;
        default: THROW;
		}
    }
    // common
    Scene->RemoveObject		(from,true,true);
    xr_delete				(from);

	ComputeBounds			();
}

void CEditShape::Detach()
{
	if (shapes.size()>1){
    	Select			(true);
        ApplyScale		();
        // create scene shapes
        const Fmatrix& M = _Transform();
        ShapeIt it=shapes.begin(); it++;
        for (; it!=shapes.end(); it++){
            string256 namebuffer;
            Scene->GenObjectName	(OBJCLASS_SHAPE, namebuffer, Name);
            CEditShape* shape 	= (CEditShape*)Scene->GetOTool(ClassID)->CreateObject(0, namebuffer);
            switch (it->type){
            case cfSphere:{
                Fsphere	T		= it->data.sphere;
                M.transform_tiny(T.P);
                shape->PPosition= T.P;
                T.P.set			(0,0,0);
                shape->add_sphere(T);
            }break;
            case cfBox:{
                Fmatrix B		= it->data.box;
                B.mulA_43		(M);
                shape->PPosition= B.c;
                B.c.set			(0,0,0);
                shape->add_box	(B);
            }break;
            default: THROW;
            }
            Scene->AppendObject	(shape,false);
	    	shape->Select		(true);
        }
        // erase shapes in base object
        it=shapes.begin(); it++;
        shapes.erase(it,shapes.end());

        ComputeBounds();

        Scene->UndoSave();
    }
}

void CEditShape::OnDetach()
{
	inherited::OnDetach	();

    m_DrawTranspColor	= SHAPE_COLOR_TRANSP;
    m_DrawEdgeColor		= SHAPE_COLOR_EDGE;
}

bool CEditShape::RayPick(float& distance, const Fvector& start, const Fvector& direction, SRayPickInfo* pinf)
{
    float dist					= distance;

	for (ShapeIt it=shapes.begin(); it!=shapes.end(); it++){
		switch (it->type){
		case cfSphere:{
            Fvector S,D;
            Fmatrix M;
            M.invert			(FTransformR);
            M.transform_dir		(D,direction);
            FITransform.transform_tiny(S,start);
            Fsphere&	T		= it->data.sphere;
            float bk_r = T.R;
//            T.R					= FScale.x;
            T.intersect			(S,D,dist);
            if (dist<=0.f)		dist = distance;

            T.R					= bk_r;
		}break;
		case cfBox:{
        	Fbox box;
            box.identity		();
            Fmatrix BI;
            BI.invert			(it->data.box);
		    Fvector S,D,S1,D1,P;
		    FITransform.transform_tiny	(S,start);
		    FITransform.transform_dir	(D,direction);
		    BI.transform_tiny			(S1,S);
		    BI.transform_dir			(D1,D);
            Fbox::ERP_Result	rp_res 	= box.Pick2(S1,D1,P);
            if (rp_res==Fbox::rpOriginOutside){
            	it->data.box.transform_tiny	(P);
                FTransform.transform_tiny	(P);
                P.sub			(start);
                dist			= P.magnitude();
            }
		}break;
		}
    }
    if (dist<distance){
        distance	= dist;
        return 		true;
    }
	return false;
}

bool CEditShape::FrustumPick(const CFrustum& frustum)
{
	const Fmatrix& M	= _Transform();
	for (ShapeIt it=shapes.begin(); it!=shapes.end(); it++){
		switch (it->type){
		case cfSphere:{
		    Fvector 	C;
            Fsphere&	T	= it->data.sphere;
		    M.transform_tiny(C,T.P);
        	if (frustum.testSphere_dirty(C,T.R*FScale.x)) return true;
		}break;
		case cfBox:{
        	Fbox 			box;
            box.identity	();
            Fmatrix B		= it->data.box;
            B.mulA_43 		(_Transform());
            box.xform		(B);
			u32 mask		= 0xff;
            if (frustum.testAABB(box.data(),mask)) return true;
		}break;
		}
    }
	return false;
}

bool CEditShape::GetBox(Fbox& box)  const
{
	if (m_Box.is_valid()){
    	box.xform(m_Box,FTransform);
    	return true;
    }
	return false;
}

bool CEditShape::LoadLTX(CInifile& ini, LPCSTR sect_name)
{
    u32 vers		= ini.r_u32(sect_name, "version");

 	inherited::LoadLTX	(ini, sect_name);

    u32 count 			= ini.r_u32			(sect_name, "shapes_count");
    if(vers>0x0001)
    	m_shape_type	= ini.r_u8			(sect_name, "shape_type");
        
    string128			buff;
    shapes.resize		(count);
    for(u32 i=0; i<count; ++i)
    {
       sprintf			(buff,"shape_type_%d", i);
       shapes[i].type	= ini.r_u8(sect_name, buff);
       if(shapes[i].type==CShapeData::cfSphere)
       {
       	sprintf			(buff,"shape_center_%d", i);
		shapes[i].data.sphere.P = ini.r_fvector3	(sect_name, buff);

       	sprintf			(buff,"shape_radius_%d", i);
		shapes[i].data.sphere.R = ini.r_float		(sect_name, buff);
       }else
       {
       	 R_ASSERT		(shapes[i].type==CShapeData::cfBox);
         sprintf			(buff,"shape_matrix_i_%d", i);
         shapes[i].data.box.i = ini.r_fvector3	(sect_name, buff);

         sprintf			(buff,"shape_matrix_j_%d", i);
         shapes[i].data.box.j = ini.r_fvector3	(sect_name, buff);

         sprintf			(buff,"shape_matrix_k_%d", i);
         shapes[i].data.box.k = ini.r_fvector3	(sect_name, buff);

         sprintf			(buff,"shape_matrix_c_%d", i);
         shapes[i].data.box.c = ini.r_fvector3	(sect_name, buff);
       }
    }


	ComputeBounds();
	return true;
}

void CEditShape::SaveLTX(CInifile& ini, LPCSTR sect_name)
{
	inherited::SaveLTX	(ini, sect_name);

	ini.w_u32			(sect_name, "version", SHAPE_CURRENT_VERSION);

    ini.w_u32			(sect_name, "shapes_count", shapes.size());
    ini.w_u8			(sect_name, "shape_type", m_shape_type);

    string128			buff;
    for(u32 i=0; i<shapes.size(); ++i)
    {
       sprintf			(buff,"shape_type_%d", i);
       ini.w_u8			(sect_name, buff, shapes[i].type);
       if(shapes[i].type==CShapeData::cfSphere)
       {
       	sprintf			(buff,"shape_center_%d", i);
		ini.w_fvector3	(sect_name, buff, shapes[i].data.sphere.P);

       	sprintf			(buff,"shape_radius_%d", i);
		ini.w_float		(sect_name, buff, shapes[i].data.sphere.R);
       }else
       {
       		R_ASSERT		(shapes[i].type==CShapeData::cfBox);
            sprintf			(buff,"shape_matrix_i_%d", i);
            ini.w_fvector3	(sect_name, buff, shapes[i].data.box.i);
            sprintf			(buff,"shape_matrix_j_%d", i);
            ini.w_fvector3	(sect_name, buff, shapes[i].data.box.j);
            sprintf			(buff,"shape_matrix_k_%d", i);
            ini.w_fvector3	(sect_name, buff, shapes[i].data.box.k);
            sprintf			(buff,"shape_matrix_c_%d", i);
            ini.w_fvector3	(sect_name, buff, shapes[i].data.box.c);
       }
    }
}

bool CEditShape::LoadStream(IReader& F)
{
	R_ASSERT(F.find_chunk(SHAPE_CHUNK_VERSION));
    u16 vers		= F.r_u16();

	inherited::LoadStream	(F);

	R_ASSERT(F.find_chunk(SHAPE_CHUNK_SHAPES));
    shapes.resize	(F.r_u32());
	F.r				(&*shapes.begin(),shapes.size()*sizeof(shape_def));

    if(F.find_chunk(SHAPE_CHUNK_DATA))
    	m_shape_type	= F.r_u8();
    
	ComputeBounds();
	return true;
}

void CEditShape::SaveStream(IWriter& F)
{
	inherited::SaveStream	(F);

	F.open_chunk	(SHAPE_CHUNK_VERSION);
	F.w_u16			(SHAPE_CURRENT_VERSION);
	F.close_chunk	();

	F.open_chunk	(SHAPE_CHUNK_SHAPES);
    F.w_u32			(shapes.size());
    F.w				(&*shapes.begin(),shapes.size()*sizeof(shape_def));
	F.close_chunk	();

    F.open_chunk	(SHAPE_CHUNK_DATA);
    F.w_u8			(m_shape_type);
	F.close_chunk	();
    
}

void CEditShape::FillProp(LPCSTR pref, PropItemVec& values)
{
	inherited::FillProp(pref,values);
	PHelper().CreateCaption	(values, PrepareKey(pref,"Shape usage"),m_shape_type==eShapeCommon?"common":"level bound");
}

void CEditShape::Render(int priority, bool strictB2F)
{
	inherited::Render(priority, strictB2F);
    if (1==priority){
        if (strictB2F){
	        EDevice.SetShader			(EDevice.m_WireShader);
            EDevice.SetRS				(D3DRS_CULLMODE,D3DCULL_NONE);
            u32 clr 					= Selected()?subst_alpha(m_DrawTranspColor, color_get_A(m_DrawTranspColor)*2):m_DrawTranspColor;
                
            Fvector zero				={0.f,0.f,0.f};
            for (ShapeIt it=shapes.begin(); it!=shapes.end(); ++it)
            {
                switch(it->type)
                {
                case cfSphere:
                {
                    Fsphere& S			= it->data.sphere;
                    Fmatrix B;
                    B.scale				(S.R,S.R,S.R);
                    B.translate_over	(S.P);
                    B.mulA_43			(_Transform());
                    if (g_bEditorDX11) {
                        Fvector wc; wc.set(B.c.x, B.c.y, B.c.z);
                        float wr = B.i.magnitude();

                        const int SLON = 16, SLAT = 12;
                        static xr_vector<FVF::L> sverts; sverts.clear();
                        sverts.reserve(SLON*SLAT*6);
                        for (int la = 0; la < SLAT; ++la) {
                            float t0 = PI*la/SLAT - PI/2.f, t1 = PI*(la+1)/SLAT - PI/2.f;
                            float y0=_sin(t0), y1=_sin(t1), r0=_cos(t0), r1=_cos(t1);
                            for (int lo = 0; lo < SLON; ++lo) {
                                float a0 = PI_MUL_2*lo/SLON, a1 = PI_MUL_2*(lo+1)/SLON;
                                float c0=_cos(a0), s0=_sin(a0), c1=_cos(a1), s1=_sin(a1);
                                Fvector da={r0*c0,y0,r0*s0}, db={r1*c0,y1,r1*s0},
                                        dcc={r1*c1,y1,r1*s1}, dd={r0*c1,y0,r0*s1};
                                Fvector pa,pb,pc,pd;
                                pa.mad(wc,da,wr); pb.mad(wc,db,wr); pc.mad(wc,dcc,wr); pd.mad(wc,dd,wr);
                                sverts.push_back({}); sverts.back().set(pa,clr);
                                sverts.push_back({}); sverts.back().set(pb,clr);
                                sverts.push_back({}); sverts.back().set(pc,clr);
                                sverts.push_back({}); sverts.back().set(pa,clr);
                                sverts.push_back({}); sverts.back().set(pc,clr);
                                sverts.push_back({}); sverts.back().set(pd,clr);
                            }
                        }
                        bool saved_blend = HW11.States.alpha_blend;
                        bool saved_dw    = HW11.States.depth_write;
                        D3D11_CULL_MODE saved_cull = HW11.States.cull_mode;
                        HW11.States.alpha_blend = true;            HW11.States.bs_dirty = true;
                        HW11.States.depth_write = false;           HW11.States.ds_dirty = true;
                        HW11.States.cull_mode   = D3D11_CULL_NONE; HW11.States.rs_dirty = true;
                        HW11.DU_DrawPrim(sverts.data(), (u32)sverts.size(), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                        HW11.States.alpha_blend = saved_blend;     HW11.States.bs_dirty = true;
                        HW11.States.depth_write = saved_dw;        HW11.States.ds_dirty = true;
                        HW11.States.cull_mode   = saved_cull;      HW11.States.rs_dirty = true;

                        u32 wire_clr = Selected() ? 0xFFFFFFFF : m_DrawEdgeColor;
                        DU_impl.DrawLineSphere(wc, wr, wire_clr, false);
                        DU_impl.DrawCross(wc, wr, wr, wr, wr, wr, wr, wire_clr, FALSE);
                    } else {
                        RCache.set_xform_world(B);
                        EDevice.SetShader(EDevice.m_WireShader);
                        DU_impl.DrawCross(zero, 1.f, m_DrawEdgeColor, false);
                        DU_impl.DrawIdentSphere(true, true, clr, m_DrawEdgeColor);
                    }
                }break;
                case cfBox:
                {
                    Fmatrix B			= it->data.box;
                    B.mulA_43			(_Transform());
                    if (g_bEditorDX11) {
                        static const Fvector corners[8] = {
                            {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},
                            {-0.5f,-0.5f, 0.5f},{0.5f,-0.5f, 0.5f},{0.5f,0.5f, 0.5f},{-0.5f,0.5f, 0.5f}
                        };
                        Fvector wc[8];
                        for (int i = 0; i < 8; i++) B.transform_tiny(wc[i], corners[i]);

                        static const int face_tri[12][3] = {
                            {0,1,2},{0,2,3},
                            {5,4,7},{5,7,6},
                            {4,0,3},{4,3,7},
                            {1,5,6},{1,6,2},
                            {4,5,1},{4,1,0},
                            {3,2,6},{3,6,7},
                        };
                        FVF::L fverts[36];
                        for (int t = 0; t < 12; t++)
                            for (int v = 0; v < 3; v++) {
                                fverts[t*3+v].p     = wc[face_tri[t][v]];
                                fverts[t*3+v].color = clr;
                            }
                        bool saved_blend = HW11.States.alpha_blend;
                        bool saved_dw    = HW11.States.depth_write;
                        D3D11_CULL_MODE saved_cull = HW11.States.cull_mode;
                        HW11.States.alpha_blend = true;          HW11.States.bs_dirty = true;
                        HW11.States.depth_write = false;         HW11.States.ds_dirty = true;
                        HW11.States.cull_mode   = D3D11_CULL_NONE; HW11.States.rs_dirty = true;
                        HW11.DU_DrawPrim(fverts, 36, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                        HW11.States.alpha_blend = saved_blend;   HW11.States.bs_dirty = true;
                        HW11.States.depth_write = saved_dw;      HW11.States.ds_dirty = true;
                        HW11.States.cull_mode   = saved_cull;    HW11.States.rs_dirty = true;

                        static const int edges[12][2] = {
                            {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
                        };
                        u32 edge_clr = Selected() ? 0xFFFFFFFF : m_DrawEdgeColor;
                        FVF::L everts[24];
                        for (int i = 0; i < 12; i++) {
                            everts[i*2+0].p = wc[edges[i][0]]; everts[i*2+0].color = edge_clr;
                            everts[i*2+1].p = wc[edges[i][1]]; everts[i*2+1].color = edge_clr;
                        }
                        HW11.DU_DrawPrim(everts, 24, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
                    } else {
                        RCache.set_xform_world(B);
                        DU_impl.DrawIdentBox(true, true, clr, m_DrawEdgeColor);
                    }
                }break;
                }
            }
            if (!g_bEditorDX11) EDevice.SetRS(D3DRS_CULLMODE,D3DCULL_CCW);
        }else{
            if( Selected()&&m_Box.is_valid() ){
		        EDevice.SetShader		(EDevice.m_SelectionShader);
                u32 clr 				= 0xFFFFFFFF;
                EDevice.SetShader		(EDevice.m_WireShader);
                if (g_bEditorDX11) {
                    Fvector pts[8]; m_Box.getpoints(pts);
                    Fbox wbox; wbox.invalidate();
                    for (int i=0; i<8; i++) {
                        Fvector wp; _Transform().transform_tiny(wp, pts[i]);
                        wbox.modify(wp);
                    }
                    DU_impl.DrawSelectionBoxB(wbox, &clr);
                } else {
                    RCache.set_xform_world(_Transform());
                    DU_impl.DrawSelectionBoxB(m_Box, &clr);
                }
            }
        }
    }
}
#include "FrameShape.h"

void CEditShape::OnFrame()
{
	inherited::OnFrame();
    if(m_shape_type==eShapeLevelBound)
    {
    	TfraShape* F 		= (TfraShape*)ParentTool->pFrame;
    	BOOL bVis = F->ebEditLevelBoundMode->Down;
    	m_RT_Flags.set(flRT_Visible, bVis);
    }
}

void CEditShape::OnShowHint(AStringVec& dest)
{
}

