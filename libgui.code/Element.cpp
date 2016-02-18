#include "libgui/Common.h"
#include "libgui/Element.h"
#include "libgui/ElementManager.h"
#include "libgui/Location.h"
#include "libgui/Layer.h"

namespace libgui
{
// Element Manager
ElementManager* Element::GetElementManager() const
{
    return _elementManager;
}

// Layer
Layer* Element::GetLayer() const
{
    return _layer;
}

// View Model
void Element::SetViewModel(shared_ptr<ViewModelBase> viewModel)
{
    _viewModel = viewModel;
}

shared_ptr<ViewModelBase> Element::GetViewModel()
{
    return _viewModel;
}

// Visual tree
shared_ptr<Element> Element::GetParent()
{
    return _parent;
}

shared_ptr<Element> Element::GetFirstChild()
{
    return _firstChild;
}

shared_ptr<Element> Element::GetLastChild()
{
    return _lastChild;
}

shared_ptr<Element> Element::GetPrevSibling()
{
    return _prevsibling;
}

shared_ptr<Element> Element::GetNextSibling()
{
    return _nextsibling;
}

void Element::RemoveChildren()
{
    // Recurse into children to thoroughly clean the element tree
    auto e = _firstChild;
    while (e != nullptr)
    {
        e->RemoveChildren();

        // Clean up pointers so that the class will be deleted
        e->_parent      = nullptr;
        e->_prevsibling = nullptr;
        auto next_e = e->_nextsibling;
        e->_nextsibling = nullptr;
        e = next_e;
    }

    _firstChild    = nullptr;
    _lastChild     = nullptr;
    _childrenCount = 0;
}

void Element::AddChild(shared_ptr<Element> element)
{
    if (_firstChild == nullptr)
    {
        _firstChild = element;
    }
    else
    {
        element->_prevsibling    = _lastChild;
        _lastChild->_nextsibling = element;
    }

    element->_parent = shared_from_this();
    _lastChild = element;

    _childrenCount++;
}

int Element::GetChildrenCount()
{
    return _childrenCount;
}

void Element::SetSingleChild(shared_ptr<Element> child)
{
    // Could be optimized to improve performance
    RemoveChildren();
    AddChild(child);
}

shared_ptr<Element> Element::GetSingleChild()
{
    if (_childrenCount == 1)
    {
        return _firstChild;
    }

    if (_childrenCount == 0)
    {
        return nullptr;
    }

    throw std::runtime_error("There is more than one child in this element");
}

// Cache Management
void Element::ClearCache(int cacheLevel)
{
    ClearElementCache(cacheLevel);

    // Recurse to children
    if (_firstChild)
    {
        for (auto e = _firstChild; e != nullptr; e = e->_nextsibling)
        {
            e->ClearCache(cacheLevel);
        }
    }
}

void Element::ClearElementCache(int cacheLevel)
{
    // This is intended to be overridden as needed for OS-specific needs.
}

// Arrangement
void Element::ResetArrangement()
{
    _left    = 0;
    _top     = 0;
    _right   = 0;
    _bottom  = 0;
    _centerX = 0;
    _centerY = 0;
    _width   = 0;
    _height  = 0;

    _isLeftSet    = false;
    _isTopSet     = false;
    _isRightSet   = false;
    _isBottomSet  = false;
    _isCenterXSet = false;
    _isCenterYSet = false;
    _isWidthSet   = false;
    _isHeightSet  = false;
}

void Element::SetSetViewModelCallback(function<void(shared_ptr<Element>)> setViewModelCallback)
{
    _setViewModelCallback = setViewModelCallback;
}

void Element::PrepareViewModel()
{
    if (_setViewModelCallback)
    {
        _setViewModelCallback(shared_from_this());
    }
    else
    {
        // By default the ViewModel is copied from the parent
        if (_parent)
        {
            _viewModel = _parent->_viewModel;
        }
    }
}

void Element::SetArrangeCallback(function<void(shared_ptr<Element>)> arrangeCallback)
{
    _arrangeCallback = arrangeCallback;
}

void Element::AddArrangeDependent(shared_ptr<Element> dependent)
{
    if (dependent->GetLayer() != GetLayer())
    {
        throw std::runtime_error("Dependent elements must be on the same layer");
    }

    _arrangeDependents.push_back(dependent);
}

void Element::Arrange()
{
    if (_arrangeCallback)
    {
        _arrangeCallback(shared_from_this());
    }
    else
    {
        // By default each element stretches
        // to fill its parent
        SetLeft(_parent->GetLeft());
        SetTop(_parent->GetTop());
        SetRight(_parent->GetRight());
        SetBottom(_parent->GetBottom());
    }
}

void Element::ArrangeAndDraw()
{
    VisitThisAndDescendents(
        [](Element* e) // What to do for each element before visiting its children
        {
            e->DoArrangeTasks();
            return e->DoDrawTasksIfVisible(boost::none); // whether or not to visit its children
        },
        [](Element* e) // What to do for each element after visiting its children
        {
            e->DoDrawTasksCleanup();
        });
}

void Element::DoArrangeTasks()
{
    ResetArrangement();
    PrepareViewModel();
    Arrange();
}

bool Element::DoDrawTasksIfVisible(const boost::optional<Rect4>& updateArea)
{
    if (GetIsVisible())
    {
        ClipToBoundsIfNeeded();
        Draw(updateArea);
        return true;
    }
    return false;
}

void Element::DoDrawTasksCleanup()
{
    if (GetIsVisible() && GetClipToBounds())
    {
        _elementManager->PopClip();
    }
}

void Element::Update()
{
    auto wasVisible = GetIsVisible();
    BeginDirtyTracking();
    {
        DoArrangeTasks();
    }
    bool moved;
    auto& redrawRegion = EndDirtyTracking(moved);

    // If the redraw region is totally hidden both before
    // and after the rearrangement, we don't have to do anything
    if ((!wasVisible && !GetIsVisible()) ||
        CoveredByLayerAbove(redrawRegion))
    {
        return;
    }

    _elementManager->PushClip(redrawRegion);
    {
        auto currentLayer = GetLayer();

        currentLayer->VisitLowerLayersIf(
            [&redrawRegion](Layer* currentLayer2)
            {
                return !currentLayer2->OpaqueAreaContains(redrawRegion);
            },
            [&redrawRegion](Layer* lowerLayer)
            {
                lowerLayer->RedrawThisAndDescendents(redrawRegion);
            });


        int thisAndAncestorClips = 0;

        VisitAncestors([&redrawRegion](Element* ancestor)
                       {
                           ancestor->DoDrawTasksIfVisible(redrawRegion);
                           if (ancestor->GetClipToBounds())
                           {
                               ++thisAndAncestorClips;
                           }
                       });

        // Apply the current element clip, if any, before drawing children
        if (ClipToBoundsIfNeeded())
        {
            ++thisAndAncestorClips;
        }

        if (moved)
        {
            // Arrange all the children of this element since it is expected that
            // children depend on their parent for arrangement
            VisitChildren([](Element* e)
                          {
                              e->ArrangeAndDraw();
                          });
        }
        else
        {
            // Element hasn't moved, so just redraw children without arranging
            VisitChildren([](Element* child)
                          {
                              child->RedrawThisAndDescendents(boost::none);
                          });
        }

        while (thisAndAncestorClips)
        {
            _elementManager->PopClip();
            --thisAndAncestorClips;
        }

        // Now draw the layers above
        currentLayer->VisitHigherLayers(
            [&redrawRegion](Layer* higherLayer)
            {
                higherLayer->RedrawThisAndDescendents(redrawRegion);
            });

    }
    _elementManager->PopClip();

    if (moved)
    {
        // update any dependent elements in this same layer
        VisitArrangeDependents([](Element* e)
                               {
                                   e->Update();
                               });
    }
}

void Element::RedrawThisAndDescendents(const boost::optional<Rect4>& redrawRegion)
{
    VisitThisAndDescendents(
        [&redrawRegion](Element* e) // What to do for each element before visiting its children
        {
            return e->DoDrawTasksIfVisible(redrawRegion);
        },
        [](Element* e) // What to do for each element after visiting its children
        {
            e->DoDrawTasksCleanup();
        });
}

void Element::InitializeAll()
{
    // Initialize the current element
    InitializeThis();

    // Then initialize all children
    if (_firstChild)
    {
        for (auto e = _firstChild; e != nullptr; e = e->_nextsibling)
        {
            e->InitializeAll();
        }
    }

}

void Element::InitializeThis()
{
    // Default behavior
    if (_parent)
    {
        // Copy the element manager from the parent
        _elementManager = _parent->_elementManager;

        // Copy the layer from the parent
        _layer = _parent->_layer;
    }
}

void Element::SetIsVisible(bool isVisible)
{
    _isVisible = isVisible;
}

bool Element::GetIsVisible()
{
    return _isVisible;
}

void Element::SetIsEnabled(bool isEnabled)
{
    _isEnabled = isEnabled;
}

bool Element::GetIsEnabled()
{
    return _isEnabled;
}

void Element::SetClipToBounds(bool clipToBounds)
{
    _clipToBounds = clipToBounds;
}

bool Element::GetClipToBounds()
{
    return _clipToBounds;
}

bool Element::ClipToBoundsIfNeeded()
{
    if (_clipToBounds)
    {
        _elementManager->PushClip(Rect4(GetLeft(), GetTop(), GetRight(), GetBottom()));

        return true;
    }

    return false;
}

void Element::SetConsumesInput(bool consumesInput)
{
    _consumesInput = consumesInput;
}

bool Element::GetConsumesInput()
{
    return _consumesInput;
}

void Element::SetLeft(double left)
{
    _isLeftSet = true;
    _left      = left;
}

void Element::SetTop(double top)
{
    _isTopSet = true;
    _top      = top;
}

void Element::SetRight(double right)
{
    _isRightSet = true;
    _right      = right;
}

void Element::SetBottom(double bottom)
{
    _isBottomSet = true;
    _bottom      = bottom;
}

void Element::SetCenterX(double centerX)
{
    _isCenterXSet = true;
    _centerX      = centerX;
}

void Element::SetCenterY(double centerY)
{
    _isCenterYSet = true;
    _centerY      = centerY;
}

void Element::SetWidth(double width)
{
    _isWidthSet = true;
    _width      = width;
}

void Element::SetHeight(double height)
{
    _isHeightSet = true;
    _height      = height;
}

HPixels Element::GetLeft()
{
    if (!_isLeftSet)
    {
        if (_isWidthSet)
        {
            if (_isRightSet)
            {
                _left = _right - _width;
            }
            else if (_isCenterXSet)
            {
                _left = _centerX - (_width / 2);
            }
        }
        _isLeftSet = true;
    }
    return HPixels(_left, _elementManager->GetDpiX());
}

VPixels Element::GetTop()
{
    if (!_isTopSet)
    {
        if (_isHeightSet)
        {
            if (_isBottomSet)
            {
                _top = _bottom - _height;
            }
            else if (_isCenterYSet)
            {
                _top = _centerY - (_height / 2);
            }
        }
        _isTopSet = true;
    }
    return VPixels(_top, _elementManager->GetDpiY());
}

HPixels Element::GetRight()
{
    if (!_isRightSet)
    {
        if (_isWidthSet)
        {
            if (_isLeftSet)
            {
                _right = _left + _width;
            }
            else if (_isCenterXSet)
            {
                _right = _centerX + (_width / 2);
            }
        }
        _isRightSet = true;
    }
    return HPixels(_right, _elementManager->GetDpiX());
}

VPixels Element::GetBottom()
{
    if (!_isBottomSet)
    {
        if (_isHeightSet)
        {
            if (_isTopSet)
            {
                _bottom = _top + _height;
            }
            else if (_isCenterYSet)
            {
                _bottom = _centerY + (_height / 2);
            }
        }
        _isBottomSet = true;
    }
    return VPixels(_bottom, _elementManager->GetDpiY());
}

HPixels Element::GetCenterX()
{
    if (!_isCenterXSet)
    {
        if (_isLeftSet && _isRightSet)
        {
            _centerX = _left + (_right - _left) / 2;
        }
        else if (_isLeftSet && _isWidthSet)
        {
            _centerX = _left + (_width / 2);
        }
        else if (_isRightSet && _isWidthSet)
        {
            _centerX = _right - (_width / 2);
        }
        _isCenterXSet = true;
    }
    return HPixels(_centerX, _elementManager->GetDpiX());
}

VPixels Element::GetCenterY()
{
    if (!_isCenterYSet)
    {
        if (_isTopSet && _isBottomSet)
        {
            _centerY = _top + (_bottom - _top) / 2;
        }
        else if (_isTopSet && _isHeightSet)
        {
            _centerY = _top + (_height / 2);
        }
        else if (_isBottomSet && _isHeightSet)
        {
            _centerY = _bottom - (_height / 2);
        }
        _isCenterYSet = true;
    }
    return VPixels(_centerY, _elementManager->GetDpiY());
}

HPixels Element::GetWidth()
{
    if (!_isWidthSet)
    {
        if (_isLeftSet && _isRightSet)
        {
            _width = _right - _left;
        }
        _isWidthSet = true;
    }
    return HPixels(_width, _elementManager->GetDpiX());
}

VPixels Element::GetHeight()
{
    if (!_isHeightSet)
    {
        if (_isTopSet && _isBottomSet)
        {
            _height = _bottom - _top;
        }
        _isHeightSet = true;
    }
    return VPixels(_height, _elementManager->GetDpiY());
}

// Drawing
void Element::Draw(const boost::optional<Rect4>& updateArea)
{
    if (_drawCallback)
    {
        _drawCallback(this, updateArea);
    }
    else
    {
        // By default no drawing takes place
    }
}

void Element::SetDrawCallback(function<void(Element*, const boost::optional<Rect4>&)> drawCallback)
{
    _drawCallback = drawCallback;
}

// Hit testing
ElementQueryInfo Element::GetElementAtPoint(const Point& point)
{
    return GetElementAtPointHelper(point, false);
}

ElementQueryInfo Element::GetElementAtPointHelper(const Point& point, bool hasDisabledAncestor)
{
    if (!GetIsVisible() || !GetConsumesInput())
    {
        return ElementQueryInfo();
    }

    // This algorithm relies on a fundamental expectation that each element's visual bounds is contained
    // by all its ancestors' visual bounds
    // Because of that, we don't have to check the child of any ancestor that falls outside of the search point

    auto& totalBounds = GetTotalBounds();

    if (point.X >= totalBounds.left && point.X <= totalBounds.right &&
        point.Y >= totalBounds.top && point.Y <= totalBounds.bottom)
    {
        if (_firstChild)
        {
            auto childrenHaveDisabledAncestor = hasDisabledAncestor || !GetIsEnabled();

            auto lastChild = FindLastChild(point);
            if (lastChild)
            {
                auto elementQueryInfo = lastChild->GetElementAtPointHelper(point, childrenHaveDisabledAncestor);
                if (elementQueryInfo.FoundElement())
                {
                    return elementQueryInfo;
                }
            }
        }
        return ElementQueryInfo(this, hasDisabledAncestor);
    }
    else
    {
        return ElementQueryInfo();
    }
}

void Element::VisitAncestors(const std::function<void(Element*)>& action)
{
    VisitAncestorsHelper(action, true);
}

void Element::VisitAncestorsHelper(const std::function<void(Element*)>& action, bool isCallee)
{
    if (_parent)
    {
        _parent->VisitAncestorsHelper(action, false);
    }

    if (!isCallee)
    {
        action(this);
    }
}

void Element::VisitChildren(const std::function<void(Element*)>& action)
{
    if (_firstChild)
    {
        for (auto e = _firstChild; e != nullptr; e = e->_nextsibling)
        {
            action(e.get());
        }
    }
}

void Element::VisitArrangeDependents(const std::function<void(Element*)> action)
{
    auto iter = _arrangeDependents.begin();
    while (iter != _arrangeDependents.end())
    {
        auto& dependentWeakPtr = *iter;
        if (auto dependent = dependentWeakPtr.lock())
        {
            action(dependent.get());
            ++iter;
        }
        else
        {
            // The dependent has disappeared so we will remove it from our list
            auto eraseIter = iter;
            ++iter;
            _arrangeDependents.erase(eraseIter);
        }
    }
}

void Element::VisitChildren(const Rect4& region, const std::function<void(Element*)>& action)
{
    // This is a plain old brute force algorithm to search all the children
    if (_firstChild)
    {
        for (auto e = _firstChild; e != nullptr; e = e->_nextsibling)
        {
            if (e->Intersects(region))
            {
                action(e.get());
            }
        }
    }
}

void Element::VisitChildrenWithTotalBounds(const Rect4& region, const std::function<void(Element*)>& action)
{
    // This is a plain old brute force algorithm to search all the children
    if (_firstChild)
    {
        for (auto e = _firstChild; e != nullptr; e = e->_nextsibling)
        {
            if (e->TotalBoundsIntersects(region))
            {
                action(e.get());
            }
        }
    }
}

void Element::VisitThisAndDescendents(const std::function<bool(Element*)> preChildrenAction,
                                      const std::function<void(Element*)> postChildrenAction)
{
    if (preChildrenAction(this))
    {
        VisitChildren([](Element* child)
                      {
                          child->VisitThisAndDescendents(preChildrenAction, postChildrenAction);
                      });

        postChildrenAction(this);
    }
}

void Element::VisitThisAndDescendents(const Rect4& region,
                                      const std::function<bool(Element*)> preChildrenAction,
                                      const std::function<void(Element*)> postChildrenAction)
{
    if (preChildrenAction(this))
    {
        VisitChildren(region,
                      [](Element* child)
                      {
                          child->VisitThisAndDescendents(preChildrenAction, postChildrenAction);
                      });

        postChildrenAction(this);
    }
}

bool Element::Intersects(const Rect4& region)
{
    // Thanks to http://stackoverflow.com/a/306332/4307047 for the rectangle intersection logic
    // but including equality with each operator so that identical rectangles would succeed
    return (region.left <= GetRight() && region.right >= GetLeft() &&
            region.top >= GetBottom() && region.bottom <= GetTop());
}

bool Element::Intersects(const Point& point)
{
    return (point.X >= GetLeft() && point.X <= GetRight() &&
            point.Y >= GetTop() && point.Y <= GetBottom());
}

bool Element::TotalBoundsIntersects(const Rect4& region)
{
    auto& totalBounds = GetTotalBounds();

    // Thanks to http://stackoverflow.com/a/306332/4307047 for the rectangle intersection logic
    // but including equality with each operator so that identical rectangles would succeed
    return (region.left <= totalBounds.right && region.right >= totalBounds.left &&
            region.top >= totalBounds.bottom && region.bottom <= totalBounds.top);
}

Element* Element::FindLastChild(const Point& point)
{
    // This is a plain old brute force algorithm to search all the children

    if (_firstChild)
    {
        for (auto e = _lastChild; e != nullptr; e = e->_prevsibling)
        {
            if (Intersects(point))
            {
                return e.get();
            }
        }
    }
    return nullptr;
}

Element::~Element()
{
}

void Element::SetLeft(Inches left)
{
    SetLeft(double(left) * _elementManager->GetDpiX());
}

void Element::SetTop(Inches top)
{
    SetTop(double(top) * _elementManager->GetDpiY());
}

void Element::SetRight(Inches right)
{
    SetRight(double(right) * _elementManager->GetDpiX());
}

void Element::SetBottom(Inches bottom)
{
    SetBottom(double(bottom) * _elementManager->GetDpiY());
}

void Element::SetCenterX(Inches centerX)
{
    SetCenterX(double(centerX) * _elementManager->GetDpiX());
}

void Element::SetCenterY(Inches centerY)
{
    SetCenterY(double(centerY) * _elementManager->GetDpiY());
}

void Element::SetWidth(Inches width)
{
    SetWidth(double(width) * _elementManager->GetDpiX());
}

void Element::SetHeight(Inches height)
{
    SetHeight(double(height) * _elementManager->GetDpiY());
}

HPixels Element::GetHPixels(Inches in)
{
    return HPixels(in, _elementManager->GetDpiX());
}

VPixels Element::GetVPixels(Inches in)
{
    return VPixels(in, _elementManager->GetDpiY());
}

ElementQueryInfo::ElementQueryInfo(Element* ElementAtPoint, bool HasDisabledAncestor)
    : ElementAtPoint(ElementAtPoint), HasDisabledAncestor(HasDisabledAncestor)
{
}

ElementQueryInfo::ElementQueryInfo()
    : ElementAtPoint(nullptr), HasDisabledAncestor(false)
{
}

bool ElementQueryInfo::FoundElement()
{
    return bool(ElementAtPoint);
}

bool Element::ThisOrAncestors(const std::function<bool(Element*)>& predicate)
{
    Element* current = this;
    do
    {
        if (predicate(current))
        {
            return true;
        }

        // Move to the next ancestor
        current = current->_parent.get();
    }
    while (current);

    return false;
}

void Element::SetVisualBounds(const Rect4& bounds)
{
    _visualBounds = bounds;
}

const boost::optional<Rect4>& Element::GetVisualBounds()
{
    return _visualBounds;
}

const Rect4 Element::GetTotalBounds()
{
    if (_visualBounds)
    {
        auto& visualBounds = _visualBounds.get();
        return Rect4(GetLeft() + visualBounds.left,
                     GetTop() + visualBounds.top,
                     GetRight() + visualBounds.right,
                     GetBottom() + visualBounds.bottom);
    }
    else
    {
        return Rect4(GetLeft(), GetTop(), GetRight(), GetBottom());
    }
}

void Element::BeginDirtyTracking()
{
    // Store the dirty bounds for later
    _dirtyBounds      = Rect4(GetLeft(), GetTop(), GetRight(), GetBottom());
    _dirtyTotalBounds = GetTotalBounds();
}

const Rect4& Element::EndDirtyTracking(bool& moved)
{
    auto currentTotalBounds = GetTotalBounds();

    auto currentBounds = Rect4(GetLeft(), GetTop(), GetRight(), GetBottom());

    moved = (currentBounds.left != _dirtyBounds.left ||
             currentBounds.top != _dirtyBounds.top ||
             currentBounds.right != _dirtyBounds.right ||
             currentBounds.bottom != _dirtyBounds.bottom);

    // Union the original bounds with the current bounds
    // This is so that if the element has moved since
    // we began dirty tracking, we'll be able to get
    // the whole dirty region, both the region that must
    // be cleared and also the region that must be drawn

    if (currentTotalBounds.left < _dirtyTotalBounds.left)
    {
        _dirtyTotalBounds.left = currentTotalBounds.left;
    }
    if (currentTotalBounds.top < _dirtyTotalBounds.top)
    {
        _dirtyTotalBounds.top = currentTotalBounds.top;
    }
    if (currentTotalBounds.right > _dirtyTotalBounds.right)
    {
        _dirtyTotalBounds.right = currentTotalBounds.right;
    }
    if (currentTotalBounds.bottom > _dirtyTotalBounds.bottom)
    {
        _dirtyTotalBounds.bottom = currentTotalBounds.bottom;
    }

    return _dirtyTotalBounds;
}

bool Element::CoveredByLayerAbove(const Rect4& region)
{
    Layer* current = GetLayer();
    while ((current = current->GetLayerAbove()))
    {
        if (current->OpaqueAreaContains(region))
        {
            return true;
        }
    }

    return false;
}

}

