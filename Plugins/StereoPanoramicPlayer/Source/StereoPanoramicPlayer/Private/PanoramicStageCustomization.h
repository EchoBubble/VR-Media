// Copyright 2019 UNAmedia. All rights reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"

class IDetailLayoutBuilder;

class FPanoramicStageCustomization : public IDetailCustomization
{
public:
    /**
    * Creates a new instance.
    *
    * @return A new property type customization.
    */
    static TSharedRef<IDetailCustomization> MakeInstance()
    {
        return MakeShareable(new FPanoramicStageCustomization());
    }

public:

    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

    void OnTypeChange();

private:

    IDetailLayoutBuilder* MyDetailLayout;
};

class FPanoramicStageNavigationCustomization : public IPropertyTypeCustomization
{
public:
    /**
    * Creates a new instance.
    *
    * @return A new property type customization.
    */
    static TSharedRef<IPropertyTypeCustomization> MakeInstance()
    {
        return MakeShareable(new FPanoramicStageNavigationCustomization());
    }

public:

    virtual void CustomizeHeader(
        TSharedRef<IPropertyHandle> PropertyHandle,
        FDetailWidgetRow& HeaderRow, 
        IPropertyTypeCustomizationUtils& CustomizationUtils) override;

    virtual void CustomizeChildren(
        TSharedRef<IPropertyHandle> PropertyHandle, 
        IDetailChildrenBuilder& ChildBuilder,
        IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:

    EVisibility GetWidgetPropertiesVisibility() const;
    bool LookupColorPropertyEnabled() const;

private:

    TSharedPtr<IPropertyHandle> WidgetClassProperty;
};

class FPanoramicStageMovieDataCustomization : public IPropertyTypeCustomization
{
public:
    /**
    * Creates a new instance.
    *
    * @return A new property type customization.
    */
    static TSharedRef<IPropertyTypeCustomization> MakeInstance()
    {
        return MakeShareable(new FPanoramicStageMovieDataCustomization());
    }

public:

    virtual void CustomizeHeader(
        TSharedRef<IPropertyHandle> PropertyHandle,
        FDetailWidgetRow& HeaderRow,
        IPropertyTypeCustomizationUtils& CustomizationUtils) override;

    virtual void CustomizeChildren(
        TSharedRef<IPropertyHandle> PropertyHandle,
        IDetailChildrenBuilder& ChildBuilder,
        IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:

    EVisibility GetDestinationStagePropertyVisibility() const;

private:

    TSharedPtr<IPropertyHandle> OnMediaEndReachedProperty;
};

#endif // WITH_EDITOR
