#include "libgui/Element.h"
#include "libgui/ElementManager.h"
#include "libgui/Location.h"
#include "libgui/Layer.h"
#include "libgui/ScopeExit.h"

#ifdef DBG
#include <typeinfo>
#endif

namespace libgui
{

Element::Dependencies::Dependencies(std::shared_ptr<Element> parent)
  : parent(parent)
{
}

Element::Element(Dependencies dependencies)
  : Element(dependencies, "Element")
{
}

Element::Element(Dependencies dependencies, std::string_view typeName)
  : _elementManager(dependencies.parent->_elementManager),
    _layer(dependencies.parent->_layer),
    _parent(dependencies.parent),
    _typeName(typeName)
{
}

// For the Layer class only
Element::Element(const LayerDependencies& layerDependencies, std::string_view typeName)
  : _elementManager(layerDependencies.elementManager),
    _parent(nullptr),
    _typeName(typeName)
{
  // Note: _layer will be set later by SetLayerToSharedFromThis()

  assert(_elementManager);
}

void Element::SetLayerFieldToSharedFromThis()
{
  _layer = std::dynamic_pointer_cast<Layer>(shared_from_this());
}

// Element Manager
ElementManager* Element::GetElementManager() const
{
  return _elementManager;
}

// Layer
std::shared_ptr<Layer> Element::GetLayer() const
{
  return _layer;
}

// View Model
void Element::SetViewModel(std::shared_ptr<ViewModelBase> viewModel)
{
  _viewModel = viewModel;
}

std::shared_ptr<ViewModelBase> Element::GetViewModel()
{
  return _viewModel;
}

// Visual tree
std::shared_ptr<Element> Element::GetParent() const
{
  return _parent;
}

std::shared_ptr<Element> Element::GetFirstChild() const
{
  return _firstChild;
}

std::shared_ptr<Element> Element::GetLastChild() const
{
  return _lastChild;
}

std::shared_ptr<Element> Element::GetPrevSibling() const
{
  return _prevsibling;
}

std::shared_ptr<Element> Element::GetNextSibling() const
{
  return _nextsibling;
}

void Element::AddChildHelper(std::shared_ptr<Element> element)
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

  _lastChild = element;

  _childrenCount++;

  // Copy the element manager to the child
  element->_elementManager = _elementManager;

  // Copy the layer to the child
  element->_layer = _layer;

}

void Element::RemoveChildren(UpdateWhenRemoving update)
{
  // Make sure that all children are updated if desired
  if (UpdateWhenRemoving::Yes == update)
  {
    VisitChildren([] (Element* child) {
      child->Update(UpdateType::Removing);
      return true;
    });
  }

  // Recurse into children to thoroughly clean the element tree
  auto e = _firstChild;
  while (e != nullptr)
  {
    // Allow subclasses to do additional cleanup
    e->OnElementIsBeingRemoved();

    e->RemoveChildren(UpdateWhenRemoving::No);

    // Clean up pointers so that the class will be deleted
    e->_parent      = nullptr;
    e->_prevsibling = nullptr;
    auto next_e = e->_nextsibling;
    e->_nextsibling = nullptr;
    e->_layer       = nullptr;

    // Remove callbacks which often capture shared pointers to other elements
    // which in turn can hold references to this element and thereby keep
    // each other alive artificially
    e->_arrangeCallback      = nullptr;
    e->_drawCallback         = nullptr;
    e->_setViewModelCallback = nullptr;

    // Prevent further updates if the class is still kept alive by other shared pointers
    e->SetIsDetached(true);

    e = next_e;
  }

  _firstChild    = nullptr;
  _lastChild     = nullptr;
  _childrenCount = 0;
}

void Element::RemoveChild(std::shared_ptr<Element> child)
{
  child->Update(UpdateType::Removing);

  // Allow subclasses to do additional cleanup
  child->OnElementIsBeingRemoved();

  auto prevSibling = child->_prevsibling;
  auto nextSibling = child->_nextsibling;

  // First cleanup the child's children
  child->RemoveChildren(UpdateWhenRemoving::No);

  // Update the child's siblings and this
  if (prevSibling)
  {
    prevSibling->_nextsibling = nextSibling;
  }
  else
  {
    // We're removing the first child
    _firstChild = nextSibling;
  }

  if (nextSibling)
  {
    nextSibling->_prevsibling = prevSibling;
  }
  else
  {
    // We're removing the last child
    _lastChild = prevSibling;
  }

  --_childrenCount;

  // Remove the child's pointers to its relatives
  child->_parent      = nullptr;
  child->_nextsibling = nullptr;
  child->_prevsibling = nullptr;
  child->_layer       = nullptr;

  // Remove callbacks which often capture shared pointers to other elements
  // which in turn can hold references to this element and thereby keep
  // each other alive artificially
  child->_arrangeCallback      = nullptr;
  child->_drawCallback         = nullptr;
  child->_setViewModelCallback = nullptr;

  // Prevent further updates if the class is still kept alive by other shared pointers
  child->SetIsDetached(true);

  // The child should disappear as soon as all shared references to it are released
}

void Element::SetIsDetached(bool isDetached)
{
  _isDetached = isDetached;
}

int Element::GetChildrenCount()
{
  return _childrenCount;
}

std::shared_ptr<Element> Element::GetSingleChild() const
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
void Element::ClearCacheAll(int cacheLevel)
{
  ClearCacheThis(cacheLevel);

  // Recurse to children
  if (_firstChild)
  {
    for (auto e = _firstChild; e != nullptr; e = e->_nextsibling)
    {
      e->ClearCacheAll(cacheLevel);
    }
  }
}

void Element::ClearCacheThis(int cacheLevel)
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

void Element::SetSetViewModelCallback(const std::function<void(std::shared_ptr<Element>)>& setViewModelCallback)
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

void Element::SetArrangeCallback(const std::function<void(std::shared_ptr<Element>)>& arrangeCallback)
{
  _arrangeCallback = arrangeCallback;
}

bool Element::ThisIsEarlierSiblingOf(Element* other)
{
  Element* nextSibling = _nextsibling.get();
  if (nextSibling == other)
  {
    return true;
  }

  if (nextSibling)
  {
    return nextSibling->ThisIsEarlierSiblingOf(other);
  }

  return false;
}

void Element::RegisterOverlappingElement(std::shared_ptr<Element> other)
{
  if (other->GetParent() != GetParent() || other->GetLayer() != GetLayer())
  {
    throw std::runtime_error("Overlapping elements must be children of the same parent element. "
                               "They must also follow the drawing order, which means only siblings added to an element "
                               "later can overlap siblings added earlier.");
  }
  else if (!ThisIsEarlierSiblingOf(other.get()))
  {
    // Note that this test, while helping verify correct API usage, also simplifies the logic below
    throw std::runtime_error("Overlapping elements must follow the drawing order, "
                               "which means only siblings added to an element "
                               "later can overlap siblings added earlier.");
  }

  if (_overlappedBy.empty())
  {
    _overlappedBy.push_back(other);
  }
  else
  {
    // Note that the preconditions checked for above help simplify this algorithm

    // Since there are already one or more overlapping elements, find the appropriate
    // place to insert into the list so that the elements maintain their drawing order.
    auto currentSibling = _nextsibling;
    auto insertPos      = _overlappedBy.begin();

    // Avoid adding the same element multiple times
    if ((*insertPos).lock() == other)
    {
      return;
    }

    while (currentSibling != other)
    {
      // Move to the next insert position whenever we pass the current insert position
      if (currentSibling == (*insertPos).lock())
      {
        ++insertPos;
        if (insertPos == _overlappedBy.end())
        {
          break;
        }

        // Avoid adding the same element multiple times
        if ((*insertPos).lock() == other)
        {
          return;
        }
      }

      currentSibling = currentSibling->_nextsibling;
    }

    // Now do the insert
    _overlappedBy.insert(insertPos, other);
  }

  // Register the other way as well
  other->RegisterOverlappedElement(shared_from_this());
}

void Element::RegisterOverlappedElement(std::shared_ptr<Element> other)
{
  if (_overlaps.empty())
  {
    _overlaps.push_back(other);
  }
  else
  {
    // Note that the preconditions checked for above help simplify this algorithm

    // Since there are already one or more overlapping elements, find the appropriate
    // place to insert into the list so that the elements maintain their drawing order.
    auto currentSibling = _prevsibling;
    auto insertPos      = _overlaps.rbegin();

    // Avoid adding the same element multiple times
    if ((*insertPos).lock() == other)
    {
      return;
    }

    while (currentSibling != other)
    {
      // Move to the prev insert position whenever we pass the current insert position
      if (currentSibling == (*insertPos).lock())
      {
        ++insertPos;
        if (insertPos == _overlaps.rend())
        {
          break;
        }

        // Avoid adding the same element multiple times
        if ((*insertPos).lock() == other)
        {
          return;
        }
      }

      currentSibling = currentSibling->_prevsibling;
    }

    // Now do the insert
    --insertPos; // reverse iterator + insert means you have to go back
    _overlaps.insert(insertPos.base(), other);
  }
}

void Element::UnregisterOverlappingElement(std::shared_ptr<Element> other)
{
  auto findIter = std::find_if(_overlappedBy.begin(), _overlappedBy.end(),
  [other] (std::weak_ptr<Element> existing) {
    return (existing.lock() == other);
  });

  if (findIter != _overlappedBy.end())
  {
    _overlappedBy.erase(findIter);
  }

  // And do the same in the opposite direction
  other->UnregisterOverlappedElement(shared_from_this());
}

void Element::UnregisterOverlappedElement(std::shared_ptr<Element> other)
{
  auto findIter = std::find_if(_overlaps.begin(), _overlaps.end(),
  [other] (std::weak_ptr<Element> existing) {
    return (existing.lock() == other);
  });

  if (findIter != _overlaps.end())
  {
    _overlaps.erase(findIter);
  }
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

void Element::ArrangeAndDrawHelper()
{
  VisitThisAndDescendents(
    [](Element* e) // What to do for each element before visiting its children
    {
      #ifdef DBG
      printf("Arranging %s\n", e->GetTypeName().c_str());
      fflush(stdout);
      #endif

      e->DoArrangeTasks();

      #ifdef DBG
      printf("Drawing %s\n", e->GetTypeName().c_str());
      fflush(stdout);
      #endif

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

  // Signal to children that the parent is being arranged
  _inArrangeMethodNow = true;
  ScopeExit onScopeExit([this] { _inArrangeMethodNow = false; });

  Arrange();
}

bool Element::DoDrawTasksIfVisible(const boost::optional<Rect4>& updateArea)
{
  if (GetIsVisible())
  {
    ClipToBoundsIfNeeded();

    // Pass to Draw only the part of the updateArea that intersects
    // with this Element, so that the Draw callbacks can make the assumption
    // that if they draw exactly in the updateArea they will not exceed their
    // own bounds.
    if (updateArea)
    {
      auto elementUpdateArea = GetTotalBounds();
      elementUpdateArea.IntersectWith(updateArea.get());
      Draw(elementUpdateArea);
    }
    else
    {
      Draw(boost::none);
    }

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

void Element::UpdateAfterAdd()
{
  Update(UpdateType::Adding);
}

void Element::UpdateAfterModify()
{
  Update(UpdateType::Modifying);
}

void Element::Update(UpdateType updateType)
{
  // Elements that have been detached from the visual tree should no longer be updated.
  if (_isDetached)
  {
    return;
  }

  if (UpdateType::Everything != updateType && !_initialUpdate)
  {
    if (UpdateType::Adding == updateType)
    {
      VisitThisAndDescendents([](Element* e) { e->_initialUpdate = true; });

      if (_parent && _parent->_inArrangeMethodNow)
      {
        // This call is coming within the Arrange method of the parent element,
        // and as a general rule, when libgui calls Arrange for an element it also
        // calls Arrange for all of its descendants right afterwards, even if
        // the Arrange method itself was the one that created those descendant(s).

        // The only exception to this rule is that if we are calling UpdateAfterModify()
        // on the parent element, libgui will choose not to update the parent's
        // descendants (this element) if the parent was not moved or resized.
        // So we have a special indicator for that case to notify the parent
        // that one or more child elements want to be arranged.
        if (_parent->_monitoringArrangeEffects)
        {
          _parent->_monitoringArrangeEffects.get().NotifyChildRequestedArrange();
        }

        // And then we do nothing for now to prevent an unnecessary double arrangements
        return;
      }
    }
    else
    {
      // Ignore all updates until one of the initial ones occurs
      return;
    }
  }

  #ifdef DBG
  printf("\n\nBeginning update cycle for %s\n", GetTypeName().c_str());
  fflush(stdout);
  #endif

  // Do the update now or as soon as possible
  _elementManager->UpdateOrAddPending(shared_from_this(), updateType);
}

void Element::UpdateHelper(UpdateType updateType)
{
  // Special case: if we are updating the whole element tree at once then
  // most of the special update logic isn't necessary and would actually
  // be a performance loss
  if (UpdateType::Everything == updateType)
  {
    ArrangeAndDrawHelper();
    _elementManager->AddToRedrawnRegion(GetTotalBounds());
    VisitThisAndDescendents([](Element* e) { e->_initialUpdate = true; });
    return;
  }


  #ifdef DBG
  printf("\nUpdating %s\n", GetTypeName().c_str());
  fflush(stdout);
  #endif

  // Arrange this element and monitor the side effects of doing so
  auto monitor = MonitorArrangeEffects(UpdateType::Adding == updateType,
    GetIsVisible(), GetBounds(), GetTotalBounds());
  {
    _monitoringArrangeEffects = monitor;
    ScopeExit onScopeExit([this] { _monitoringArrangeEffects = boost::none; });

    DoArrangeTasks();
  }
  auto arrangeEffects = monitor.Finish(GetIsVisible(), GetBounds(), GetTotalBounds());

  auto& redrawRegion = arrangeEffects.GetUnionedTotalBounds();


  if (arrangeEffects.WasInvisibleBeforeAndAfter() ||
      !GetAreAncestorsVisible() ||
      CoveredByLayerAbove(redrawRegion))
  {
    return;
  }

  #ifdef DBG
  printf("Calculated redraw region as (%f, %f, %f, %f)\n",
         redrawRegion.left, redrawRegion.top, redrawRegion.right, redrawRegion.bottom);
  fflush(stdout);
  #endif

  _elementManager->PushClip(redrawRegion);
  {
    auto currentLayer = GetLayer().get();

    int thisAndAncestorClips = 0;

    if (UpdateType::Adding != updateType || currentLayer->AnyLayersAbove())
    {
      // Before we do any drawing we need to make sure that we have all the
      // ancestor clips pushed.  This duplicates the clipping that is done
      // later by ancestors' DoDrawTasksIfVisible but I don't want to
      // change that and I don't think it makes any real performance impact.
      // Also, the amount of explicit clipping tends to be minimal in most
      // applications so I think this is reasonable.  Finally if we are using
      // IntersectionStack to manage this stack then it will not even forward
      // the new clip on if it is the same as the previous.
      VisitAncestors(
        [&redrawRegion, &thisAndAncestorClips](Element* ancestor) {
          if (ancestor->GetIsVisible() && ancestor->ClipToBoundsIfNeeded())
          {
            ++thisAndAncestorClips;
          }
        });


      currentLayer->VisitLowerLayersIf(
        [this, &redrawRegion, currentLayer, updateType](Layer* currentLayer2) {
          // Special case: if we are removing this layer then always
          // visit lower layers regardless of the opaque area
          if (currentLayer2 == currentLayer &&
              currentLayer == this &&
              UpdateType::Removing == updateType)
          {
            return true;
          }
          return !currentLayer2->OpaqueAreaContains(redrawRegion);
        },
        [&redrawRegion](Layer* lowerLayer) {
          #ifdef DBG
          printf("Redrawing lower layer\n");
          fflush(stdout);
          #endif

          lowerLayer->RedrawThisAndDescendents(redrawRegion);
        });

      VisitAncestors(
        [&redrawRegion, &thisAndAncestorClips](Element* ancestor) {
          #ifdef DBG
          printf("Redrawing ancestor %s\n", ancestor->GetTypeName().c_str());
          fflush(stdout);
          #endif

          bool visible = ancestor->DoDrawTasksIfVisible(redrawRegion);
          if (visible && ancestor->GetClipToBounds())
          {
            ++thisAndAncestorClips;
          }
        });

      // Make sure overlapped elements and their children are drawn next
      VisitOverlappedElements([&redrawRegion](Element* e) {
        e->RedrawThisAndDescendents(redrawRegion);
      });
    }

    // Now draw this element and its children unless it's invisible
    // or it's going to be removed
    if (GetIsVisible() && UpdateType::Removing != updateType)
    {
      // Apply the current element clip, if any, before drawing this and children
      if (ClipToBoundsIfNeeded())
      {
        ++thisAndAncestorClips;
      }

      #ifdef DBG
      printf("Drawing %s\n", GetTypeName().c_str());
      fflush(stdout);
      #endif

      Draw(boost::none);

      if (UpdateType::Adding == updateType ||
          arrangeEffects.ElementWasMovedOrResized() ||
          arrangeEffects.ElementBecameVisible() || // because if this is the
                                                   // first time it's visible
                                                   // its children will never
                                                   // have been arranged
          arrangeEffects.ChildrenRequestedArrange() ||
          GetUpdateRearrangesDescendants())
      {
        #ifdef DBG
        printf("Rearranging all children of %s\n", GetTypeName().c_str());
        fflush(stdout);
        #endif

        // Arrange and draw all the children of this element
        VisitChildren([](Element* e) {
          e->ArrangeAndDrawHelper();
          return true;
        });
      }
      else
      {
        #ifdef DBG
        printf("Redrawing all children of %s\n", GetTypeName().c_str());
        fflush(stdout);
        #endif

        // Element hasn't moved, so just redraw children without arranging
        VisitChildren([](Element* child) {
          child->RedrawThisAndDescendents(boost::none);
          return true;
        });
      }
    }

    if (UpdateType::Modifying == updateType)
    {
      // Make sure overlapping elements and their children are drawn on top
      VisitOverlappingElements([&redrawRegion](Element* e) {
        e->RedrawThisAndDescendents(redrawRegion);
      });
    }

    while (thisAndAncestorClips)
    {
      _elementManager->PopClip();
      --thisAndAncestorClips;
    }

    // Now draw the layers above
    currentLayer->VisitHigherLayers(
      [&redrawRegion](Layer* higherLayer) {
        #ifdef DBG
        printf("Redrawing higher layer\n");
        fflush(stdout);
        #endif

        higherLayer->RedrawThisAndDescendents(redrawRegion);
      });

  }
  _elementManager->PopClip();

  _elementManager->AddToRedrawnRegion(redrawRegion);
}

void Element::RedrawThisAndDescendents(const boost::optional<Rect4>& redrawRegion)
{
  VisitThisAndDescendents(
    [&redrawRegion](Element* e) // What to do for each element before visiting its children
    {
      if (redrawRegion && !e->TotalBoundsIntersects(redrawRegion.get()))
      {
        // Ignore any element hierarchy that doesn't intersect with the redraw region
        return false;
      }

      #ifdef DBG
      printf("Redrawing this or descendent %s\n", e->GetTypeName().c_str());
      fflush(stdout);
      #endif

      return e->DoDrawTasksIfVisible(redrawRegion);
    },
    [](Element* e) // What to do for each element after visiting its children
    {
      e->DoDrawTasksCleanup();
    });
}

void Element::SetIsVisible(bool isVisible)
{
  _isVisible = isVisible;
}

bool Element::GetIsVisible()
{
  return _isVisible;
}

bool Element::GetAreAncestorsVisible()
{
  auto parent = GetParent();
  if (parent)
  {
    return !parent->ThisOrAncestors([](Element* e) { return !e->GetIsVisible(); });
  }

  // It seems that the API is most simple if we return true when there are no ancestors
  return true;
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

Rect4 Element::GetBounds()
{
  return Rect4(GetLeft(), GetTop(), GetRight(), GetBottom());
}

void Element::SetTouchMargin(const Rect4& margin)
{
  _touchMargin = margin;
}

const Rect4& Element::GetTouchMargin() const
{
  return _touchMargin;
}

// Drawing
void Element::Draw(const boost::optional<Rect4>& updateArea)
{
  if (_drawCallback)
  {
    #ifdef DBG
    printf("Draw callback for %s\n", GetTypeName().c_str());
    fflush(stdout);
    #endif

    _drawCallback(this, updateArea);
  }
  else
  {
    // By default no drawing takes place
  }
}

void Element::OnElementIsBeingRemoved()
{
}

void Element::SetDrawCallback(const std::function<void(Element*, const boost::optional<Rect4>&)>& drawCallback)
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
  if (!GetIsVisible() || (!GetConsumesInput() && 0 == GetChildrenCount()))
  {
    return ElementQueryInfo();
  }

  // This algorithm relies on a fundamental expectation that each element's bounds is contained
  // by all its ancestors' bounds
  // Because of that, we don't have to check the child of any ancestor that falls outside of the search point

  if (Intersects(point))
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
    if (GetConsumesInput())
    {
      return ElementQueryInfo(this, hasDisabledAncestor);
    }
  }

  // No matches here
  return ElementQueryInfo();
}

bool Element::GetElementInRect(const Rect4& hitRect, FuzzyHitQuery& hitQuery)
{
  return GetElementInRectHelper(hitRect, hitQuery, false);
}

bool Element::GetElementInRectHelper(
  const Rect4& hitRect, FuzzyHitQuery& hitQuery, bool hasDisabledAncestor)
{
  if (!GetIsVisible() || (!GetConsumesInput() && 0 == GetChildrenCount()))
  {
    return false;
  }

  // This algorithm relies on a fundamental expectation that each element's bounds is contained
  // by all its ancestors' bounds
  // Because of that, we don't have to check the child of any ancestor that falls outside of the search point

  if (TouchIntersects(hitRect))
  {
    if (_firstChild)
    {
      auto childrenHaveDisabledAncestor = hasDisabledAncestor || !GetIsEnabled();

      VisitLastChildren(hitRect,
      [&hitRect, &hitQuery, childrenHaveDisabledAncestor] (Element* child) {
        child->GetElementInRectHelper(hitRect, hitQuery, childrenHaveDisabledAncestor);
        // Stop visiting children if we have a 'trumping' match
        return !hitQuery.FoundFiftyPercent();
      });
      if (hitQuery.FoundFiftyPercent())
      {
        // We already have a 'trumping' match, so stop looking
        return true;
      }
    }

    if (GetConsumesInput() && (_layer.get() != this))
    {
      // No children match, but we already know that this element intersects
      Rect4 bounds = GetBounds();
      Rect4 intersectionArea = hitRect;
      intersectionArea.IntersectWith(bounds);

      auto matchingPercent = intersectionArea.Area() / hitRect.Area();

      // If this match is more complete than the previous one
      // (or this is the first match), then replace the previous one as the
      // max matching element
      if (matchingPercent > hitQuery.MaxMatchingPercent ||
          !hitQuery.MaxMatchingElement.FoundElement() )
      {
        hitQuery.MaxMatchingPercent = matchingPercent;
        hitQuery.MaxMatchingElement = ElementQueryInfo(this, hasDisabledAncestor);
      }
    }

    // This element does intersect
    return true;
  }

  return false;
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

void Element::VisitOverlappingElements(const std::function<void(Element*)>& action)
{
  auto iter = _overlappedBy.begin();
  while (iter != _overlappedBy.end())
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
      _overlappedBy.erase(eraseIter);
    }
  }
}

void Element::VisitOverlappedElements(const std::function<void(Element*)>& action)
{
  auto iter = _overlaps.begin();
  while (iter != _overlaps.end())
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
      _overlaps.erase(eraseIter);
    }
  }
}

void Element::VisitChildren(const Rect4& region, const std::function<bool(Element*)>& action)
{
  // This default is a plain old brute force algorithm to search all the children
  if (_firstChild)
  {
    for (auto e = _firstChild; e != nullptr; e = e->_nextsibling)
    {
      if (e->Intersects(region))
      {
        if (!action(e.get()))
        {
          // The action returned false, so stop
          return;
        }
      }
    }
  }
}

void Element::VisitLastChildren(const Rect4& region, const std::function<bool(Element*)>& action)
{
  // This default is a plain old brute force algorithm to search all the children
  if (_firstChild)
  {
    for (auto e = _lastChild; e != nullptr; e = e->_prevsibling)
    {
      if (e->Intersects(region))
      {
        if (!action(e.get()))
        {
          // The action returned false, so stop
          return;
        }
      }
    }
  }
}

void Element::VisitChildrenWithTotalBounds(const Rect4& region, const std::function<bool(Element*)>& action)
{
  // This default is a plain old brute force algorithm to search all the children
  if (_firstChild)
  {
    for (auto e = _firstChild; e != nullptr; e = e->_nextsibling)
    {
      if (e->TotalBoundsIntersects(region))
      {
        if (!action(e.get()))
        {
          return;
        }
      }
    }
  }
}

void Element::VisitThisAndDescendents(const std::function<bool(Element*)>& preChildrenAction,
                                      const std::function<void(Element*)>& postChildrenAction)
{
  if (preChildrenAction(this))
  {
    VisitChildren([&preChildrenAction, &postChildrenAction](Element* child) {
      child->VisitThisAndDescendents(preChildrenAction, postChildrenAction);
      return true;
    });

    postChildrenAction(this);
  }
}

void Element::VisitThisAndDescendents(const Rect4& region,
                                      const std::function<bool(Element*)>& preChildrenAction,
                                      const std::function<void(Element*)>& postChildrenAction)
{
  if (preChildrenAction(this))
  {
    VisitChildren(region,
                  [&preChildrenAction, &postChildrenAction](Element* child) {
                    child->VisitThisAndDescendents(preChildrenAction, postChildrenAction);
                    return true;
                  });

    postChildrenAction(this);
  }
}

void Element::VisitThisAndDescendents(const std::function<void(Element*)>& action)
{
  VisitChildren([&action](Element* child) {
    child->VisitThisAndDescendents(action);
    return true;
  });

  action(this);
}

bool Element::Intersects(const Rect4& region)
{
  // Thanks to http://stackoverflow.com/a/306332/4307047 for the rectangle intersection logic
  // but including equality with each operator so that identical rectangles would succeed,
  // and also flipping the comparisons for top and bottom since we're using top-down coordinates
  return (region.left <= GetRight() && region.right >= GetLeft() &&
          region.top <= GetBottom() && region.bottom >= GetTop());
}

bool Element::TouchIntersects(const Rect4& region)
{
  auto left   = GetLeft()   + _touchMargin.left;
  auto top    = GetTop()    + _touchMargin.top;
  auto right  = GetRight()  - _touchMargin.right;
  auto bottom = GetBottom() - _touchMargin.bottom;
  // Thanks to http://stackoverflow.com/a/306332/4307047 for the rectangle intersection logic
  // but including equality with each operator so that identical rectangles would succeed,
  // and also flipping the comparisons for top and bottom since we're using top-down coordinates
  return (region.left <= right && region.right >= left &&
          region.top <= bottom && region.bottom >= top);
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
  // and also flipping the comparisons for top and bottom since we're using top-down coordinates
  return (region.left <= totalBounds.right && region.right >= totalBounds.left &&
          region.top <= totalBounds.bottom && region.bottom >= totalBounds.top);
}

Element* Element::FindLastChild(const Point& point)
{
  // This is a plain old brute force algorithm to search all the children

  if (_firstChild)
  {
    for (auto e = _lastChild; e != nullptr; e = e->_prevsibling)
    {
      if (e->Intersects(point))
      {
        return e.get();
      }
    }
  }
  return nullptr;
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

HPixels Element::GetHPixels(double px)
{
  return HPixels(px, _elementManager->GetDpiX());
}

VPixels Element::GetVPixels(double px)
{
  return VPixels(px, _elementManager->GetDpiY());
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

FuzzyHitQuery::FuzzyHitQuery()
  : MaxMatchingPercent(0.0)
{
}

bool FuzzyHitQuery::FoundFiftyPercent() const
{
  return MaxMatchingPercent >= 0.5;
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

void Element::SetVisualBounds(const boost::optional<Rect4>& bounds)
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
    return _visualBounds.get();
  }
  else
  {
    return GetBounds();
  }
}

bool Element::CoveredByLayerAbove(const Rect4& region)
{
  auto current = GetLayer();
  while ((current = current->GetLayerAbove()))
  {
    if (current->OpaqueAreaContains(region))
    {
      return true;
    }
  }

  return false;
}

std::string_view Element::GetTypeName() const
{
  return _typeName;
}

void Element::SetTypeName(std::string_view name)
{
  _typeName = name;
}

void Element::SetUpdateRearrangesDescendants(bool updateRearrangesDescendents)
{
  _updateRearrangesDescendents = updateRearrangesDescendents;
}

bool Element::GetUpdateRearrangesDescendants()
{
  return _updateRearrangesDescendents;
}

Element::MonitorArrangeEffects::MonitorArrangeEffects(
  bool addingElement,
  bool originallyVisible,
  const Rect4& originalBounds,
  const Rect4& originalTotalBounds
  ) :
  addingElement(addingElement),
  originallyVisible(originallyVisible),
  originalBounds(originalBounds),
  originalTotalBounds(originalTotalBounds),
  childrenRequestedArrange(false)
{
}

void Element::MonitorArrangeEffects::NotifyChildRequestedArrange()
{
  childrenRequestedArrange = true;
}

Element::ArrangeEffects Element::MonitorArrangeEffects::Finish(bool currentlyVisible,
                                                               const Rect4& currentBounds,
                                                               const Rect4& currentTotalBounds) const
{
  ArrangeEffects result;

  if (addingElement)
  {
    // If this is the first arrange ever, then ignore the previous state
    result.wasInvisibleBeforeAndAfter = !currentlyVisible;
    result.elementWasMovedOrResized = true;
    result.elementBecameVisible = true;
    result.unionedTotalBounds = currentTotalBounds;
  }
  else
  {
    result.wasInvisibleBeforeAndAfter = !originallyVisible && !currentlyVisible;
    result.elementBecameVisible       = !originallyVisible &&  currentlyVisible;

    result.elementWasMovedOrResized =
      (currentBounds.left != originalBounds.left ||
       currentBounds.top != originalBounds.top ||
       currentBounds.right != originalBounds.right ||
       currentBounds.bottom != originalBounds.bottom);

    // Union the original bounds with the current bounds
    // This is so that if the element has moved since
    // we began dirty tracking, we'll be able to get
    // the whole dirty region, both the region that must
    // be cleared and also the region that must be drawn

    result.unionedTotalBounds = originalTotalBounds;

    if (currentTotalBounds.left < result.unionedTotalBounds.left)
    {
      result.unionedTotalBounds.left = currentTotalBounds.left;
    }
    if (currentTotalBounds.top < result.unionedTotalBounds.top)
    {
      result.unionedTotalBounds.top = currentTotalBounds.top;
    }
    if (currentTotalBounds.right > result.unionedTotalBounds.right)
    {
      result.unionedTotalBounds.right = currentTotalBounds.right;
    }
    if (currentTotalBounds.bottom > result.unionedTotalBounds.bottom)
    {
      result.unionedTotalBounds.bottom = currentTotalBounds.bottom;
    }
  }

  result.childrenRequestedArrange = childrenRequestedArrange;

  return result;
}

bool Element::ArrangeEffects::WasInvisibleBeforeAndAfter() const
{
  return wasInvisibleBeforeAndAfter;
}

bool Element::ArrangeEffects::ElementWasMovedOrResized() const
{
  return elementWasMovedOrResized;
}

bool Element::ArrangeEffects::ElementBecameVisible() const
{
  return elementBecameVisible;
}

const Rect4& Element::ArrangeEffects::GetUnionedTotalBounds() const
{
  return unionedTotalBounds;
}

bool Element::ArrangeEffects::ChildrenRequestedArrange() const
{
  return childrenRequestedArrange;
}
}

