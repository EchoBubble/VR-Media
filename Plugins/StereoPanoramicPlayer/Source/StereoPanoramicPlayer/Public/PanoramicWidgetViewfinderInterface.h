// Copyright 2019 UNAmedia. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PanoramicWidgetViewfinderInterface.generated.h"



class APanoramicDirector;



UINTERFACE(MinimalAPI)
class UPanoramicWidgetViewfinderInterface : public UInterface
{
	GENERATED_BODY()
};




/**
Panoramic UMG Widget Viewfinder interface.

If the UMG Widget used as viewfinder with APanoramicDirector implements this interface,
it will have access to enhanced functionalities.
*/
class STEREOPANORAMICPLAYER_API IPanoramicWidgetViewfinderInterface
{
	GENERATED_BODY()

public:
	/** 
	 * Callback invoked to initialize a viewfinder instance.
	 *
	 * @param[in] Director The APanoramicDirector that is managing this viewfinder.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Panoramic")
	void InitViewfinder(APanoramicDirector * Director);
};
