//
// Created by sjm-kociskobd on 10/8/15.
//

#pragma once

#include <libgui/AnimatableValue.h>
#include <libgui/Types.h>

class Length: public AnimatableValue
{

public:
    Length(double value);
    Length(libgui::HPixels& hPixels);
    Length(libgui::VPixels& vPixels);
};



