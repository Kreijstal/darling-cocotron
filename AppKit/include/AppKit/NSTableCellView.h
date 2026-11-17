/*
 This file is part of Darling.

 Copyright (C) 2019 Lubos Dolezel

 Darling is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Darling is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#import <AppKit/NSView.h>
#import <AppKit/AppKitIvarLayout.h>
#import <AppKit/NSCell.h>
#import <AppKit/NSTableView.h>

@class NSTextField, NSImageView;

@interface NSTableCellView : NSView {
    NSTextField *_textField;
    NSImageView *_imageView;
    id _objectValue;
    NSBackgroundStyle _backgroundStyle;
    NSTableViewRowSizeStyle _rowSizeStyle;
    // Apple's footprint ends in one word; it holds the pointer to the extras.
    NS_EXTRA_IVARS_BEGIN(NSTableCellView)
    NSTextField *_secondTextField;
    NS_EXTRA_IVARS_END(NSTableCellView)
}

@property (assign) IBOutlet NSTextField *textField;
// Darling/Cocotron extension: convenience secondary label outlet used by GrandPerspective
// and modern two-line subtitle table cell prototypes.
@property (assign) IBOutlet NSTextField *secondTextField;
@property (assign) IBOutlet NSImageView *imageView;
@property (retain) id objectValue;
@property NSBackgroundStyle backgroundStyle;
@property NSTableViewRowSizeStyle rowSizeStyle;

@end
