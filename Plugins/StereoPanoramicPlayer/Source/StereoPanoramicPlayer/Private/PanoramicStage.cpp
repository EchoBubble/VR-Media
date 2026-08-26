// Copyright 2019 UNAmedia. All rights reserved.

#include "PanoramicStage.h"

UPanoramicStage::UPanoramicStage(const FObjectInitializer& ObjectInitializer)
{
}



FPanoramicStageNavigation::FPanoramicStageNavigation()
	: WidgetScale (1.f),
	  LookupColor (FColor::Black),
	  YawRotationSetFlag (false),
	  YawRotation (0.f)
{
}
