// Copyright PocketBook - Interview Task

#pragma once

namespace PocketBook {

// Interface that provides notifications for any long operation
struct IProgressNotifier
{
    virtual ~IProgressNotifier() = default;
    virtual void init( int _min, int _max ) = 0;
    virtual void notifyProgress( int _current ) = 0;
};

} // namespace PocketBook