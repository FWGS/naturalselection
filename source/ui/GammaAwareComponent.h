//======== (C) Copyright 2002 Charles G. Cleveland All rights reserved. =========
//
// The copyright to the contents herein is the property of Charles G. Cleveland.
// The contents may be used and/or copied only with the written permission of
// Charles G. Cleveland, or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile: GammaAwareComponent.h $
// $Date: 2002/08/16 02:28:55 $
//
//-------------------------------------------------------------------------------
// $Log: GammaAwareComponent.h,v $
// Revision 1.2  2002/08/16 02:28:55  Flayra
// - Added document headers
//
//===============================================================================
#ifndef GAMMAAWARECOMPONENT_H
#define GAMMAAWARECOMPONENT_H

class GammaAwareComponent
{
public:
	virtual void NotifyGammaChange(float inGammaSlope) = 0;
	
};

#endif
