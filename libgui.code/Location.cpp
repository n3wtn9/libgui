//
// Created by sjm-kociskobd on 10/8/15.
//

#include "libgui/Location.h"

Location::Location()
    : Location(0.0, 0.0)
{
}

Location::Location(Length& x, Length& y)
    : x(x), y(y)
{
}
