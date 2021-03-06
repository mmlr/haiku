/*
 * Copyright 2013, Stephan Aßmus <superstippi@gmx.de>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef TEXT_DOCUMENT_VIEW_H
#define TEXT_DOCUMENT_VIEW_H

#include <String.h>
#include <View.h>

#include "TextDocument.h"
#include "TextDocumentLayout.h"


class TextDocumentView : public BView {
public:
								TextDocumentView(const char* name = NULL);
	virtual						~TextDocumentView();

	virtual void				Draw(BRect updateRect);

	virtual	void				AttachedToWindow();
	virtual void				FrameResized(float width, float height);

	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseUp(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);

	virtual	BSize				MinSize();
	virtual	BSize				MaxSize();
	virtual	BSize				PreferredSize();

	virtual	bool				HasHeightForWidth();
	virtual	void				GetHeightForWidth(float width, float* min,
									float* max, float* preferred);

			void				SetTextDocument(
									const TextDocumentRef& document);

			void				SetInsets(float inset);
			void				SetInsets(float horizontal, float vertical);
			void				SetInsets(float left, float top, float right,
									float bottom);

			void				SetCaret(const BPoint& where,
									bool extendSelection);

private:
			float				_TextLayoutWidth(float viewWidth) const;

			void				_UpdateScrollBars();

			void				_SetCaretOffset(int32 offset, bool updateAnchor,
									bool lockSelectionAnchor);

			void				_GetSelectionShape(BShape& shape,
									int32 start, int32 end);

private:
			TextDocumentRef		fTextDocument;
			TextDocumentLayout	fTextDocumentLayout;

			float				fInsetLeft;
			float				fInsetTop;
			float				fInsetRight;
			float				fInsetBottom;

			int32				fSelectionAnchorOffset;
			int32				fCaretOffset;
			float				fCaretAnchorX;
			bool				fShowCaret;

			bool				fMouseDown;
};

#endif // TEXT_DOCUMENT_VIEW_H
