/* Haiku window system selection support.
   Copyright (C) 2021-2022 Free Software Foundation, Inc.

This file is part of GNU Emacs.

GNU Emacs is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

GNU Emacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#include "lisp.h"
#include "blockinput.h"
#include "coding.h"
#include "haikuselect.h"
#include "haikuterm.h"

#include <stdlib.h>

static Lisp_Object
haiku_selection_data_1 (Lisp_Object clipboard)
{
  Lisp_Object result = Qnil;
  char *targets[256];

  block_input ();
  if (EQ (clipboard, QPRIMARY))
    BClipboard_primary_targets ((char **) &targets, 256);
  else if (EQ (clipboard, QSECONDARY))
    BClipboard_secondary_targets ((char **) &targets, 256);
  else if (EQ (clipboard, QCLIPBOARD))
    BClipboard_system_targets ((char **) &targets, 256);
  else
    {
      unblock_input ();
      signal_error ("Bad clipboard", clipboard);
    }

  for (int i = 0; targets[i]; ++i)
    {
      result = Fcons (build_unibyte_string (targets[i]),
		      result);
      free (targets[i]);
    }
  unblock_input ();

  return result;
}

DEFUN ("haiku-selection-targets", Fhaiku_selection_targets,
       Shaiku_selection_targets, 1, 1, 0,
       doc: /* Find the types of data available from CLIPBOARD.
CLIPBOARD should be the symbol `PRIMARY', `SECONDARY' or `CLIPBOARD'.
Return the available types as a list of strings.  */)
  (Lisp_Object clipboard)
{
  return haiku_selection_data_1 (clipboard);
}

DEFUN ("haiku-selection-data", Fhaiku_selection_data, Shaiku_selection_data,
       2, 2, 0,
       doc: /* Retrieve content typed as NAME from the clipboard
CLIPBOARD.  CLIPBOARD is the symbol `PRIMARY', `SECONDARY' or
`CLIPBOARD'.  NAME is a MIME type denoting the type of the data to
fetch.  */)
  (Lisp_Object clipboard, Lisp_Object name)
{
  CHECK_SYMBOL (clipboard);
  CHECK_STRING (name);
  char *dat;
  ssize_t len;

  block_input ();
  if (EQ (clipboard, QPRIMARY))
    dat = BClipboard_find_primary_selection_data (SSDATA (name), &len);
  else if (EQ (clipboard, QSECONDARY))
    dat = BClipboard_find_secondary_selection_data (SSDATA (name), &len);
  else if (EQ (clipboard, QCLIPBOARD))
    dat = BClipboard_find_system_data (SSDATA (name), &len);
  else
    {
      unblock_input ();
      signal_error ("Bad clipboard", clipboard);
    }
  unblock_input ();

  if (!dat)
    return Qnil;

  Lisp_Object str = make_unibyte_string (dat, len);

  /* `foreign-selection' just means that the selection has to be
     decoded by `gui-get-selection'.  It has no other meaning,
     AFAICT.  */
  Fput_text_property (make_fixnum (0), make_fixnum (len),
		      Qforeign_selection, Qt, str);

  block_input ();
  BClipboard_free_data (dat);
  unblock_input ();

  return str;
}

DEFUN ("haiku-selection-put", Fhaiku_selection_put, Shaiku_selection_put,
       3, 4, 0,
       doc: /* Add or remove content from the clipboard CLIPBOARD.
CLIPBOARD is the symbol `PRIMARY', `SECONDARY' or `CLIPBOARD'.  NAME
is a MIME type denoting the type of the data to add.  DATA is the
string that will be placed in the clipboard, or nil if the content is
to be removed.  CLEAR, if non-nil, means to erase all the previous
contents of the clipboard.  */)
  (Lisp_Object clipboard, Lisp_Object name, Lisp_Object data,
   Lisp_Object clear)
{
  CHECK_SYMBOL (clipboard);
  CHECK_STRING (name);
  if (!NILP (data))
    CHECK_STRING (data);

  block_input ();
  char *dat = !NILP (data) ? SSDATA (data) : NULL;
  ptrdiff_t len = !NILP (data) ? SBYTES (data) : 0;

  if (EQ (clipboard, QPRIMARY))
    BClipboard_set_primary_selection_data (SSDATA (name), dat, len,
					   !NILP (clear));
  else if (EQ (clipboard, QSECONDARY))
    BClipboard_set_secondary_selection_data (SSDATA (name), dat, len,
					     !NILP (clear));
  else if (EQ (clipboard, QCLIPBOARD))
    BClipboard_set_system_data (SSDATA (name), dat, len, !NILP (clear));
  else
    {
      unblock_input ();
      signal_error ("Bad clipboard", clipboard);
    }
  unblock_input ();

  return Qnil;
}

DEFUN ("haiku-selection-owner-p", Fhaiku_selection_owner_p, Shaiku_selection_owner_p,
       0, 1, 0,
       doc: /* Whether the current Emacs process owns the given SELECTION.
The arg should be the name of the selection in question, typically one
of the symbols `PRIMARY', `SECONDARY', or `CLIPBOARD'.  For
convenience, the symbol nil is the same as `PRIMARY', and t is the
same as `SECONDARY'.  */)
  (Lisp_Object selection)
{
  bool value;

  if (NILP (selection))
    selection = QPRIMARY;
  else if (EQ (selection, Qt))
    selection = QSECONDARY;

  block_input ();
  if (EQ (selection, QPRIMARY))
    value = BClipboard_owns_primary ();
  else if (EQ (selection, QSECONDARY))
    value = BClipboard_owns_secondary ();
  else if (EQ (selection, QCLIPBOARD))
    value = BClipboard_owns_clipboard ();
  else
    value = false;
  unblock_input ();

  return value ? Qt : Qnil;
}

void
syms_of_haikuselect (void)
{
  DEFSYM (QSECONDARY, "SECONDARY");
  DEFSYM (QCLIPBOARD, "CLIPBOARD");
  DEFSYM (QSTRING, "STRING");
  DEFSYM (QUTF8_STRING, "UTF8_STRING");
  DEFSYM (Qforeign_selection, "foreign-selection");
  DEFSYM (QTARGETS, "TARGETS");

  defsubr (&Shaiku_selection_data);
  defsubr (&Shaiku_selection_put);
  defsubr (&Shaiku_selection_targets);
  defsubr (&Shaiku_selection_owner_p);
}
