/* Interface.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Interface.h"

#include "DataNode.h"
#include "Font.h"
#include "FontSet.h"
#include "GameData.h"
#include "Information.h"
#include "LineShader.h"
#include "OutlineShader.h"
#include "Panel.h"
#include "Rectangle.h"
#include "RingShader.h"
#include "Screen.h"
#include "Sprite.h"
#include "SpriteSet.h"
#include "SpriteShader.h"
#include "UI.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {
	// Parse a set of tokens that specify horizontal and vertical alignment.
	void ParseAlignment(const DataNode &node, Point *alignment, int i = 1)
	{
		for( ; i < node.Size(); ++i)
		{
			if(node.Token(i) == "left")
				alignment->X() = -1.;
			else if(node.Token(i) == "top")
				alignment->Y() = -1.;
			else if(node.Token(i) == "right")
				alignment->X() = 1.;
			else if(node.Token(i) == "bottom")
				alignment->Y() = 1.;
			else
				node.PrintTrace("Unrecognized interface element alignment:");
		}
	}
}



// Destructor, which frees the memory used by the polymorphic list of elements.
Interface::~Interface()
{
	for(Element *element : elements)
		delete element;
}



// Load an interface.
void Interface::Load(const DataNode &node)
{
	// Skip unnamed interfaces.
	if(node.Size() < 2)
		return;
	
	// First, figure out the alignment of this interface.
	ParseAlignment(node, &alignment, 2);
	
	// Now, parse the elements in it.
	string visibleIf;
	string activeIf;
	for(const DataNode &child : node)
	{
		if((child.Token(0) == "point" || child.Token(0) == "box") && child.Size() >= 2)
		{
			// This node specifies a named point where custom drawing is done.
			points[child.Token(1)].Load(child, alignment);
		}
		else if(child.Token(0) == "visible" || child.Token(0) == "active")
		{
			// This node alters the visibility or activation of future nodes.
			string &str = (child.Token(0) == "visible" ? visibleIf : activeIf);
			if(child.Size() >= 3 && child.Token(1) == "if")
				str = child.Token(2);
			else
				str.clear();
		}
		else
		{
			// Check if this node specifies a known element type.
			if(child.Token(0) == "sprite" || child.Token(0) == "image" || child.Token(0) == "outline")
				elements.push_back(new ImageElement(child, alignment));
			else if(child.Token(0) == "label" || child.Token(0) == "string" || child.Token(0) == "button")
				elements.push_back(new TextElement(child, alignment));
			else if(child.Token(0) == "bar" || child.Token(0) == "ring")
				elements.push_back(new BarElement(child, alignment));
			else
			{
				child.PrintTrace("Unrecognized interface element:");
				continue;
			}
			
			// If we get here, a new element was just added.
			elements.back()->SetConditions(visibleIf, activeIf);
		}
	}
}



// Draw this interface.
void Interface::Draw(const Information &info, Panel *panel) const
{
	// Figure out the anchor point, which may be a corner, the center of an edge
	// of the screen, or the center of the screen.
	Point anchor = .5 * Screen::Dimensions() * alignment;
	
	for(const Element *element : elements)
		element->DrawAt(anchor, info, panel);
}



// Check if a named point exists.
bool Interface::HasPoint(const string &name) const
{
	return points.count(name);
}



// Get the center of the named point.
Point Interface::GetPoint(const string &name) const
{
	auto it = points.find(name);
	if(it == points.end())
		return Point();
	
	return it->second.Bounds().Center() + .5 * Screen::Dimensions() * alignment;
}



// Get the dimensions of the named point.
Point Interface::GetSize(const string &name) const
{
	auto it = points.find(name);
	if(it == points.end())
		return Point();
	
	return it->second.Bounds().Dimensions();
}



Rectangle Interface::GetBox(const string &name) const
{
	auto it = points.find(name);
	if(it == points.end())
		return Rectangle();
	
	return it->second.Bounds() + .5 * Screen::Dimensions() * alignment;
}



// Members of the Element base class:

// Create a new element. The alignment of the interface that contains
// this element is used to calculate the element's position.
void Interface::Element::Load(const DataNode &node, const Point &globalAlignment)
{
	// Even if the global alignment is not centered, we switch to treating it as
	// if it is centered if the object's position is specified as a "center."
	bool isCentered = !globalAlignment;
	
	// Assume that the subclass constructor already parsed this line of data.
	for(const DataNode &child : node)
	{
		// Check if this token will change the width or height.
		bool hasDimensions = (child.Token(0) == "dimensions" && child.Size() >= 3);
		bool hasWidth = hasDimensions | (child.Token(0) == "width" && child.Size() >= 2);
		bool hasHeight = hasDimensions | (child.Token(0) == "height" && child.Size() >= 2);
		
		if(child.Token(0) == "align" && child.Size() > 1)
		{
			ParseAlignment(child, &alignment);
		}
		else if(hasWidth || hasHeight)
		{
			// If this line modifies the width or height, the center of the
			// element may need to be shifted depending on the global alignment
			// and the previous value of its width or height.
			if(hasWidth)
			{
				double newWidth = child.Value(1);
				Point center = bounds.Center();
				Point dimensions = bounds.Dimensions();
				// If this object has a "center", ignore global alignment.
				if(!isCentered)
					center.X() += .5 * globalAlignment.X() * (dimensions.X() - newWidth);
				dimensions.X() = newWidth;
				bounds = Rectangle(center, dimensions);
			}
			if(hasHeight)
			{
				double newHeight = child.Value(1 + hasDimensions);
				Point center = bounds.Center();
				Point dimensions = bounds.Dimensions();
				// If this object has a "center", ignore global alignment.
				if(!isCentered)
					center.Y() += .5 * globalAlignment.Y() * (dimensions.Y() - newHeight);
				dimensions.Y() = newHeight;
				bounds = Rectangle(center, dimensions);
			}
		}
		else if(child.Token(0) == "center" && child.Size() >= 3)
		{
			// This object should ignore the global alignment.
			isCentered = true;
			// Center the bounding box on the given point.
			bounds = Rectangle(Point(child.Value(1), child.Value(2)), bounds.Dimensions());
		}
		else if(child.Token(0) == "from" && child.Size() >= 6 && child.Token(3) == "to")
		{
			// Create a bounding box stretching between the two given points.
			bounds = Rectangle::WithCorners(
				Point(child.Value(1), child.Value(2)),
				Point(child.Value(4), child.Value(5)));
		}
		else if(child.Token(0) == "from" && child.Size() >= 3)
		{
			// The bounding box extends outwards from the given point in a
			// direction determined by the global aligment.
			bounds = Rectangle(
				Point(child.Value(1), child.Value(2)) - .5 * alignment * bounds.Dimensions(),
				bounds.Dimensions());
		}
		else if(child.Token(0) == "pad" && child.Size() >= 3)
		{
			// Add this much padding when aligning the object within its bounding box.
			padding = Point(child.Value(1), child.Value(2));
		}
		else if(!ParseLine(child))
			child.PrintTrace("Unrecognized interface element attribute:");
	}
}



// Draw this element, relative to the given anchor point. If this is a
// button, it will add a clickable zone to the given panel.
void Interface::Element::DrawAt(const Point &anchor, const Information &info, Panel *panel) const
{
	if(!info.HasCondition(visibleIf))
		return;
	
	// Get the bounding box of this element, relative to the anchor point.
	Rectangle box = bounds + anchor;
	// Check if this element is active.
	int state = info.HasCondition(activeIf);
	// Check if the mouse is hovering over this element.
	state += (state && box.Contains(UI::GetMouse()));
	// Place buttons even if they are inactive, in case the UI wants to show a
	// message explaining why the button is inactive.
	if(panel)
		Place(box, panel);
	
	// Figure out how the element should be aligned within its bounding box.
	Point nativeDimensions = NativeDimensions(info, state);
	Point slack = .5 * (bounds.Dimensions() - nativeDimensions) - padding;
	Rectangle rect(bounds.Center() + anchor + alignment * slack, nativeDimensions);
	
	Draw(rect, info, state);
}



// Set the conditions that control when this element is visible and active.
// An empty string means it is always visible or active.
void Interface::Element::SetConditions(const string &visible, const string &active)
{
	visibleIf = visible;
	activeIf = active;
}



// Get the bounding rectangle, relative to the anchor point.
const Rectangle &Interface::Element::Bounds() const
{
	return bounds;
}



// Parse the given data line: one that is not recognized by Element
// itself. This returns false if it does not recognize the line, either.
bool Interface::Element::ParseLine(const DataNode &node)
{
	return false;
}



// Report the actual dimensions of the object that will be drawn.
Point Interface::Element::NativeDimensions(const Information &info, int state) const
{
	return bounds.Dimensions();
}



// Draw this element in the given rectangle.
void Interface::Element::Draw(const Rectangle &rect, const Information &info, int state) const
{
}



// Add any click handlers needed for this element. This will only be
// called if the element is visible and active.
void Interface::Element::Place(const Rectangle &bounds, Panel *panel) const
{
}



// Members of the ImageElement class:

// Constructor.
Interface::ImageElement::ImageElement(const DataNode &node, const Point &globalAlignment)
{
	if(node.Size() < 2)
		return;
	
	// Remember whether this is an outline element.
	isOutline = (node.Token(0) == "outline");
	// If this is a "sprite," look up the sprite with the given name. Otherwise,
	// the sprite path will be dynamically supplied by the Information object.
	if(node.Token(0) == "sprite")
		sprite[Element::ACTIVE] = SpriteSet::Get(node.Token(1));
	else
		name = node.Token(1);
	
	// This function will call ParseLine() for any line it does not recognize.
	Load(node, globalAlignment);
	
	// Fill in any undefined state sprites.
	if(sprite[Element::ACTIVE])
	{
		if(!sprite[Element::INACTIVE])
			sprite[Element::INACTIVE] = sprite[Element::ACTIVE];
		if(!sprite[Element::HOVER])
			sprite[Element::HOVER] = sprite[Element::ACTIVE];
	}
}



// Parse the given data line: one that is not recognized by Element
// itself. This returns false if it does not recognize the line, either.
bool Interface::ImageElement::ParseLine(const DataNode &node)
{
	// The "inactive" and "hover" sprite only applies to non-dynamic images.
	// The "colored" tag only applies to outlines.
	if(node.Token(0) == "inactive" && node.Size() >= 2 && name.empty())
		sprite[Element::INACTIVE] = SpriteSet::Get(node.Token(1));
	else if(node.Token(0) == "hover" && node.Size() >= 2 && name.empty())
		sprite[Element::HOVER] = SpriteSet::Get(node.Token(1));
	else if(isOutline && node.Token(0) == "colored")
		isColored = true;
	else
		return false;
	
	return true;
}



// Report the actual dimensions of the object that will be drawn.
Point Interface::ImageElement::NativeDimensions(const Information &info, int state) const
{
	const Sprite *sprite = GetSprite(info, state);
	if(!sprite || !sprite->Width() || !sprite->Height())
		return Point();
	
	Point size(sprite->Width(), sprite->Height());
	if(!bounds.Dimensions())
		return size;
	
	// If one of the dimensions is zero, it means the sprite's size is not
	// constrained in that dimension.
	double xScale = !bounds.Width() ? 1000. : bounds.Width() / size.X();
	double yScale = !bounds.Height() ? 1000. : bounds.Height() / size.Y();
	return size * min(xScale, yScale);
}



// Draw this element in the given rectangle.
void Interface::ImageElement::Draw(const Rectangle &rect, const Information &info, int state) const
{
	const Sprite *sprite = GetSprite(info, state);
	if(!sprite || !sprite->Width() || !sprite->Height())
		return;
	
	if(isOutline)
	{
		Color color = (isColored ? info.GetOutlineColor() : Color(1., 1.));
		Point unit = info.GetSpriteUnit(name);
		int frame = info.GetSpriteFrame(name);
		OutlineShader::Draw(sprite, rect.Center(), rect.Dimensions(), color, unit, frame);
	}
	else
		SpriteShader::Draw(sprite, rect.Center(), rect.Width() / sprite->Width());
}



const Sprite *Interface::ImageElement::GetSprite(const Information &info, int state) const
{
	return name.empty() ? sprite[state] : info.GetSprite(name);
}



// Members of the TextElement class:

// Constructor.
Interface::TextElement::TextElement(const DataNode &node, const Point &globalAlignment)
{
	if(node.Size() < 2)
		return;
	
	isDynamic = (node.Token(0) == "string");
	if(node.Token(0) == "button")
	{
		buttonKey = node.Token(1).front();
		if(node.Size() >= 3)
			str = node.Token(2);
	}
	else
		str = node.Token(1);
	
	// This function will call ParseLine() for any line it does not recognize.
	Load(node, globalAlignment);
	
	// Fill in any undefined state colors. By default labels are "medium", strings
	// are "bright", and button brightness depends on its activation state.
	if(!color[Element::ACTIVE] && !buttonKey)
		color[Element::ACTIVE] = GameData::Colors().Get(isDynamic ? "bright" : "medium");
	
	if(!color[Element::ACTIVE])
	{
		// If no color is specified and this is a button, use the default colors.
		color[Element::ACTIVE] = GameData::Colors().Get("active");
		if(!color[Element::INACTIVE])
			color[Element::INACTIVE] = GameData::Colors().Get("inactive");
		if(!color[Element::HOVER])
			color[Element::HOVER] = GameData::Colors().Get("hover");
	}
	else
	{
		// If a base color was specified, also use it for any unspecified states.
		if(!color[Element::INACTIVE])
			color[Element::INACTIVE] = color[Element::ACTIVE];
		if(!color[Element::HOVER])
			color[Element::HOVER] = color[Element::ACTIVE];
	}
}



// Parse the given data line: one that is not recognized by Element
// itself. This returns false if it does not recognize the line, either.
bool Interface::TextElement::ParseLine(const DataNode &node)
{
	if(node.Token(0) == "size" && node.Size() >= 2)
		fontSize = node.Value(1);
	else if(node.Token(0) == "color" && node.Size() >= 2)
		color[Element::ACTIVE] = GameData::Colors().Get(node.Token(1));
	else if(node.Token(0) == "inactive" && node.Size() >= 2)
		color[Element::INACTIVE] = GameData::Colors().Get(node.Token(1));
	else if(node.Token(0) == "hover" && node.Size() >= 2)
		color[Element::HOVER] = GameData::Colors().Get(node.Token(1));
	else
		return false;
	
	return true;
}



// Report the actual dimensions of the object that will be drawn.
Point Interface::TextElement::NativeDimensions(const Information &info, int state) const
{
	const Font &font = FontSet::Get(fontSize);
	string text = GetString(info);
	return Point(font.Width(text), font.Height());
}



// Draw this element in the given rectangle.
void Interface::TextElement::Draw(const Rectangle &rect, const Information &info, int state) const
{
	// Avoid crashes for malformed interface elements that are not fully loaded.
	if(!color[state])
		return;
	
	FontSet::Get(fontSize).Draw(GetString(info), rect.TopLeft(), *color[state]);
}



// Add any click handlers needed for this element. This will only be
// called if the element is visible and active.
void Interface::TextElement::Place(const Rectangle &bounds, Panel *panel) const
{
	if(buttonKey && panel)
		panel->AddZone(bounds, buttonKey);
}



string Interface::TextElement::GetString(const Information &info) const
{
	return (isDynamic ? info.GetString(str) : str);
}



// Members of the BarElement class:

// Constructor.
Interface::BarElement::BarElement(const DataNode &node, const Point &globalAlignment)
{
	if(node.Size() < 2)
		return;
	
	// Get the name of the element and find out what type it is (bar or ring).
	name = node.Token(1);
	isRing = (node.Token(0) == "ring");
	
	// This function will call ParseLine() for any line it does not recognize.
	Load(node, globalAlignment);
	
	// Fill in a default color if none is specified.
	if(!color)
		color = GameData::Colors().Get("active");
}



// Parse the given data line: one that is not recognized by Element
// itself. This returns false if it does not recognize the line, either.
bool Interface::BarElement::ParseLine(const DataNode &node)
{
	if(node.Token(0) == "color" && node.Size() >= 2)
		color = GameData::Colors().Get(node.Token(1));
	else if(node.Token(0) == "size" && node.Size() >= 2)
		width = node.Value(1);
	else
		return false;
	
	return true;
}



// Draw this element in the given rectangle.
void Interface::BarElement::Draw(const Rectangle &rect, const Information &info, int state) const
{
	// Get the current settings for this bar or ring.
	double value = info.BarValue(name);
	double segments = info.BarSegments(name);
	if(segments <= 1.)
		segments = 0.;
	
	// Avoid crashes for malformed interface elements that are not fully loaded.
	if(!color || !width || !value)
		return;
	
	if(isRing)
	{
		if(!rect.Width() || !rect.Height())
			return;
		
		RingShader::Draw(rect.Center(), .5 * rect.Width(), width, value, *color, segments > 1. ? segments : 0.);
	}
	else
	{
		// Figue out where the line should be drawn from and to.
		// Note: this assumes that the bottom of the rectangle is the start.
		Point start = rect.BottomRight();
		Point dimensions = -rect.Dimensions();
		double length = dimensions.Length();
		
		// We will have (segments - 1) gaps between the segments.
		double empty = segments ? (width / length) : 0.;
		double filled = segments ? (1. - empty * (segments - 1.)) / segments : 1.;
		
		// Draw segments until we've drawn the desired length.
		double v = 0.;
		while(v < value)
		{
			Point from = start + v * dimensions;
			v += filled;
			Point to = start + min(v, value) * dimensions;
			v += empty;
			
			LineShader::Draw(from, to, width, *color);
		}
	}
}
