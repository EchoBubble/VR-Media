// Copyright 2019 UNAmedia. All rights reserved.

#include "PanoramicStageCustomization.h"

#if WITH_EDITOR

#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "DetailWidgetRow.h"

DECLARE_DELEGATE_OneParam(FCustomizationDelegate, IDetailPropertyRow&);

// this utility function is used to fill all struct properties 
void FillOutChildren(
    TSharedRef<IPropertyHandle> PropertyHandle,
    IDetailChildrenBuilder& ChildBuilder,
    IPropertyTypeCustomizationUtils& CustomizationUtils,
    TMap<FName, FCustomizationDelegate>& Customizations)
{
    // Generate all the other children
    uint32 NumChildren;
    PropertyHandle->GetNumChildren(NumChildren);

    for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
    {
        TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
        if (ChildHandle->GetProperty() == NULL)
        {
            FillOutChildren(ChildHandle, ChildBuilder, CustomizationUtils, Customizations);
        }
        else
        {
            FName Name = ChildHandle->GetProperty()->GetFName();
            auto& Row = ChildBuilder.AddProperty(ChildHandle);

            // customize the row if needed
            if (Customizations.Contains(Name))
                Customizations[Name].Execute(Row);
        }
    }
}

void FPanoramicStageCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
    MyDetailLayout = &DetailLayout;

    TArray< TWeakObjectPtr<UObject> > Objects;
    DetailLayout.GetObjectsBeingCustomized(Objects);
    if (Objects.Num() != 1)
    {
        // I don't know what to do here or what could be expected. Just return to disable all templating functionality
        return;
    }

    TSharedPtr<IPropertyHandle> TypePropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UPanoramicStage, MediaType), UPanoramicStage::StaticClass());

    FSimpleDelegate UpdateTypeDelegate = FSimpleDelegate::CreateSP(this, &FPanoramicStageCustomization::OnTypeChange);
    TypePropertyHandle->SetOnPropertyValueChanged(UpdateTypeDelegate);

    // Hide properties where necessary
    UPanoramicStage* Obj = Cast<UPanoramicStage>(Objects[0].Get());
    if (Obj)
    {
        TSharedPtr<IPropertyHandle> MovieDataProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UPanoramicStage, MovieData), UPanoramicStage::StaticClass());
        TSharedPtr<IPropertyHandle> ImageDataProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UPanoramicStage, ImageData), UPanoramicStage::StaticClass());

        if (Obj->MediaType == EPanoramicMediaType::Movie)
        {
           DetailLayout.HideProperty(ImageDataProperty);
        }
        else if (Obj->MediaType == EPanoramicMediaType::Image)
        {
            DetailLayout.HideProperty(MovieDataProperty);
        }
        else
        {
            check(false); // this shouldn't happen
        }
    }
}

void FPanoramicStageCustomization::OnTypeChange()
{
    check(MyDetailLayout);
    MyDetailLayout->ForceRefreshDetails();
}

///////////////////////////////////////////////////////////

void FPanoramicStageNavigationCustomization::CustomizeHeader(
    TSharedRef<IPropertyHandle> PropertyHandle,
    FDetailWidgetRow& HeaderRow,
    IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    HeaderRow.NameContent()
        [
            PropertyHandle->CreatePropertyNameWidget()
        ];
}

void FPanoramicStageNavigationCustomization::CustomizeChildren(
    TSharedRef<IPropertyHandle> PropertyHandle, 
    IDetailChildrenBuilder& ChildBuilder, 
    IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    auto WidgetClassHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPanoramicStageNavigation, WidgetClass));
    WidgetClassProperty = WidgetClassHandle.ToSharedRef();

    FCustomizationDelegate WidgetPropertiesCustomization;
    WidgetPropertiesCustomization.BindLambda([&](IDetailPropertyRow& Row)
    {
        Row.Visibility(TAttribute<EVisibility>::Create(
            TAttribute<EVisibility>::FGetter::CreateSP(this, &FPanoramicStageNavigationCustomization::GetWidgetPropertiesVisibility)));
    });

    FCustomizationDelegate LookupColorCustomization;
    LookupColorCustomization.BindLambda([&](IDetailPropertyRow& Row)
    {
        Row.IsEnabled(TAttribute<bool>::Create(
            TAttribute<bool>::FGetter::CreateSP(this, &FPanoramicStageNavigationCustomization::LookupColorPropertyEnabled)));
    });

    TMap<FName, FCustomizationDelegate> Customizations;
    Customizations.Add(GET_MEMBER_NAME_CHECKED(FPanoramicStageNavigation, WidgetPosition), WidgetPropertiesCustomization);
    Customizations.Add(GET_MEMBER_NAME_CHECKED(FPanoramicStageNavigation, WidgetScale), WidgetPropertiesCustomization);
    Customizations.Add(GET_MEMBER_NAME_CHECKED(FPanoramicStageNavigation, LookupColor), LookupColorCustomization);

    FillOutChildren(PropertyHandle, ChildBuilder, CustomizationUtils, Customizations);
}

EVisibility FPanoramicStageNavigationCustomization::GetWidgetPropertiesVisibility() const
{
    check(WidgetClassProperty);

    UObject* ResourceObject;
    WidgetClassProperty->GetValue(ResourceObject);
    return ResourceObject != nullptr ? EVisibility::Visible : EVisibility::Hidden;
}

bool FPanoramicStageNavigationCustomization::LookupColorPropertyEnabled() const
{
    check(WidgetClassProperty);

    auto Parent = WidgetClassProperty->GetParentHandle();    
    
    if (!Parent->IsValidHandle())
        return true;

    TArray<UObject*> Objects;
    Parent->GetOuterObjects(Objects);

    if (Objects.Num() != 1)
        return true;

    auto PanoramicStage = Cast<UPanoramicStage>(Objects[0]);
    if (!PanoramicStage)
        return true;

    return PanoramicStage->NavigationLookupTexture != nullptr;
}

///////////////////////////////////////////////////////////

void FPanoramicStageMovieDataCustomization::CustomizeHeader(
    TSharedRef<IPropertyHandle> PropertyHandle,
    FDetailWidgetRow& HeaderRow,
    IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    HeaderRow.NameContent()
        [
            PropertyHandle->CreatePropertyNameWidget()
        ];
}

void FPanoramicStageMovieDataCustomization::CustomizeChildren(
    TSharedRef<IPropertyHandle> PropertyHandle,
    IDetailChildrenBuilder& ChildBuilder,
    IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    OnMediaEndReachedProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPanoramicStageMovieData, OnMediaEndReachedDefaultBehaviour)).ToSharedRef();
     
    FCustomizationDelegate MyCustomization;
    MyCustomization.BindLambda([&](IDetailPropertyRow& Row) 
    {
        Row.Visibility(TAttribute<EVisibility>::Create(
            TAttribute<EVisibility>::FGetter::CreateSP(this, &FPanoramicStageMovieDataCustomization::GetDestinationStagePropertyVisibility)));
    });

    TMap<FName, FCustomizationDelegate> Customizations;
    Customizations.Add(GET_MEMBER_NAME_CHECKED(FPanoramicStageMovieData, DestinationStageOnMediaEndReached), MyCustomization);

    FillOutChildren(PropertyHandle, ChildBuilder, CustomizationUtils, Customizations);
}

EVisibility FPanoramicStageMovieDataCustomization::GetDestinationStagePropertyVisibility() const
{
    check(OnMediaEndReachedProperty);

    uint8 Value;
    OnMediaEndReachedProperty->GetValue(Value);

    if (Value == (uint8)EOnMediaEndReachedDefaultBehaviour::GoToStage)
        return EVisibility::Visible;
    else
        return EVisibility::Hidden;
}

#endif // WITH_EDITOR
