#pragma once
//-
// ===========================================================================
// Copyright 2024 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
// ===========================================================================
//+
//
// CLASS:    MFnOpenPBRSurfaceShader
//
// ****************************************************************************

#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MFnDependencyNode.h>

OPENMAYA_MAJOR_NAMESPACE_OPEN

// ****************************************************************************
// CLASS DECLARATION (MFnOpenPBRSurfaceShader)

OPENMAYA_AVAILABLE(2025.3)
//! \ingroup OpenMaya MFn
//! \brief Manage OpenPBR Surface Shaders. 
/*!
 MFnOpenPBRSurfaceShader facilitates the creation and manipulation of
 dependency graph nodes representing OpenPBR surface shaders.
*/
class OPENMAYA_EXPORT MFnOpenPBRSurfaceShader : public MFnDependencyNode
{
	declareMFn( MFnOpenPBRSurfaceShader, MFnDependencyNode );

public:
	MObject 	create( bool UIvisible = true, MStatus * ReturnStatus = NULL );
	float	baseWeight( MStatus * ReturnStatus = NULL ) const;
	MStatus	setBaseWeight( const float& base_weight );
	MColor	baseColor( MStatus * ReturnStatus = NULL ) const;
	MStatus	setBaseColor( const MColor& base_color );
	float	baseDiffuseRoughness( MStatus * ReturnStatus = NULL ) const;
	MStatus	setBaseDiffuseRoughness( const float& base_diffuse_roughness );
	float	baseMetalness( MStatus * ReturnStatus = NULL ) const;
	MStatus	setBaseMetalness( const float& base_metalness );
	float	specularWeight( MStatus * ReturnStatus = NULL ) const;
	MStatus	setSpecularWeight( const float& specular_weight );
	MColor	specularColor( MStatus * ReturnStatus = NULL ) const;
	MStatus	setSpecularColor( const MColor& specular_color );
	float	specularRoughness( MStatus * ReturnStatus = NULL ) const;
	MStatus	setSpecularRoughness( const float& specular_roughness );
	float	specularIOR( MStatus * ReturnStatus = NULL ) const;
	MStatus	setSpecularIOR( const float& specular_i_o_r );
	float	specularRoughnessAnisotropy( MStatus * ReturnStatus = NULL ) const;
	MStatus	setSpecularRoughnessAnisotropy( const float& specular_roughness_anisotropy );
	float	transmissionWeight( MStatus * ReturnStatus = NULL ) const;
	MStatus	setTransmissionWeight( const float& transmissionWeight );
	MColor	transmissionColor( MStatus * ReturnStatus = NULL ) const;
	MStatus	setTransmissionColor( const MColor& transmission_color );
	float	transmissionDepth( MStatus * ReturnStatus = NULL ) const;
	MStatus	setTransmissionDepth( const float& transmission_depth );
	MColor	transmissionScatter( MStatus * ReturnStatus = NULL ) const;
	MStatus	setTransmissionScatter( const MColor& transmission_scatter );
	float	transmissionScatterAnisotropy( MStatus * ReturnStatus = NULL ) const;
	MStatus	setTransmissionScatterAnisotropy( const float& transmission_scatter_anisotropy );
	float	transmissionDispersionScale( MStatus * ReturnStatus = NULL ) const;
	MStatus	setTransmissionDispersionScale( const float& transmission_dispersion_scale );
	float	transmissionDispersionAbbeNumber( MStatus * ReturnStatus = NULL ) const;
	MStatus	setTransmissionDispersionAbbeNumber( const float& transmission_dispersion_abbe_number );
	float	subsurfaceWeight( MStatus * ReturnStatus = NULL ) const;
	MStatus	setSubsurfaceWeight( const float& subsurface_weight );
	MColor	subsurfaceColor( MStatus * ReturnStatus = NULL ) const;
	MStatus	setSubsurfaceColor( const MColor& subsurface_color );
	float	subsurfaceRadius( MStatus * ReturnStatus = NULL ) const;
	MStatus	setSubsurfaceRadius( const float& subsurface_radius );
	MColor	subsurfaceRadiusScale( MStatus * ReturnStatus = NULL ) const;
	MStatus	setSubsurfaceRadiusScale( const MColor& subsurface_radius_scale );
	float	subsurfaceScatterAnisotropy( MStatus * ReturnStatus = NULL ) const;
	MStatus	setSubsurfaceScatterAnisotropy( const float& subsurface_scatter_anisotropy );
	float	fuzzWeight( MStatus * ReturnStatus = NULL ) const;
	MStatus	setFuzzWeight( const float& fuzz_weight );
	MColor	fuzzColor( MStatus * ReturnStatus = NULL ) const;
	MStatus	setFuzzColor( const MColor& fuzz_color );
	float	fuzzRoughness( MStatus * ReturnStatus = NULL ) const;
	MStatus	setFuzzRoughness( const float& fuzz_roughness );
	float	coatWeight( MStatus * ReturnStatus = NULL ) const;
	MStatus	setCoatWeight( const float& coat_weight );
	MColor	coatColor( MStatus * ReturnStatus = NULL ) const;
	MStatus	setCoatColor( const MColor& coat_color );
	float	coatRoughness( MStatus * ReturnStatus = NULL ) const;
	MStatus	setCoatRoughness( const float& coat_roughness );
	float	coatRoughnessAnisotropy( MStatus * ReturnStatus = NULL ) const;
	MStatus	setCoatRoughnessAnisotropy( const float& coat_roughness_anisotropy );
	float	coatIOR( MStatus * ReturnStatus = NULL ) const;
	MStatus	setCoatIOR( const float& coat_i_o_r );
	float	coatDarkening( MStatus * ReturnStatus = NULL ) const;
	MStatus	setCoatDarkening( const float& coat_darkening );
	float	thinFilmWeight( MStatus * ReturnStatus = NULL ) const;
	MStatus	setThinFilmWeight( const float& thin_film_weight );
	float	thinFilmThickness( MStatus * ReturnStatus = NULL ) const;
	MStatus	setThinFilmThickness( const float& thin_film_thickness );
	float	thinFilmIOR( MStatus * ReturnStatus = NULL ) const;
	MStatus	setThinFilmIOR( const float& thin_film_i_o_r );
	float	emissionLuminance( MStatus * ReturnStatus = NULL ) const;
	MStatus	setEmissionLuminance( const float& emission_luminance );
	MColor	emissionColor( MStatus * ReturnStatus = NULL ) const;
	MStatus	setEmissionColor( const MColor& emission_color );
	float	geometryOpacity( MStatus * ReturnStatus = NULL ) const;
	MStatus	setGeometryOpacity( const float& geometry_opacity );
	bool	geometryThinWalled( MStatus * ReturnStatus = NULL ) const;
	MStatus	setGeometryThinWalled( const bool& geometry_thin_walled );

BEGIN_NO_SCRIPT_SUPPORT:

 	declareMFnConstConstructor( MFnOpenPBRSurfaceShader, MFnDependencyNode );

END_NO_SCRIPT_SUPPORT:

protected:
// No protected members

private:
// No private members
};

OPENMAYA_NAMESPACE_CLOSE
