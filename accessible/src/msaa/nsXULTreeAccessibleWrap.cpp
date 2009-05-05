/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Leventhal
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXULTreeAccessibleWrap.h"

#include "nsIBoxObject.h"
#include "nsTextFormatter.h"
#include "nsIFrame.h"

// --------------------------------------------------------
// nsXULTreeAccessibleWrap
// --------------------------------------------------------

nsXULTreeAccessibleWrap::nsXULTreeAccessibleWrap(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsXULTreeAccessible(aDOMNode, aShell)
{
}

nsresult
nsXULTreeAccessibleWrap::GetRoleInternal(PRUint32 *aRole)
{
  NS_ENSURE_STATE(mTree);

  nsCOMPtr<nsITreeColumns> cols;
  mTree->GetColumns(getter_AddRefs(cols));
  nsCOMPtr<nsITreeColumn> primaryCol;
  if (cols) {
    cols->GetPrimaryColumn(getter_AddRefs(primaryCol));
  }
  // No primary column means we're in a list
  // In fact, history and mail turn off the primary flag when switching to a flat view
  *aRole = primaryCol ? nsIAccessibleRole::ROLE_OUTLINE :
                        nsIAccessibleRole::ROLE_LIST;

  return NS_OK;
}

// --------------------------------------------------------
// nsXULTreeitemAccessibleWrap Accessible
// --------------------------------------------------------

nsXULTreeitemAccessibleWrap::nsXULTreeitemAccessibleWrap(nsIAccessible *aParent, 
                                                         nsIDOMNode *aDOMNode, 
                                                         nsIWeakReference *aShell, 
                                                         PRInt32 aRow, 
                                                         nsITreeColumn* aColumn) :
nsXULTreeitemAccessible(aParent, aDOMNode, aShell, aRow, aColumn)
{
}

nsresult
nsXULTreeitemAccessibleWrap::GetRoleInternal(PRUint32 *aRole)
{
  // No primary column means we're in a list
  // In fact, history and mail turn off the primary flag when switching to a flat view
  NS_ENSURE_STATE(mColumn);
  PRBool isPrimary = PR_FALSE;
  mColumn->GetPrimary(&isPrimary);
  *aRole = isPrimary ? nsIAccessibleRole::ROLE_OUTLINEITEM :
                       nsIAccessibleRole::ROLE_LISTITEM;

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeitemAccessibleWrap::GetBounds(PRInt32 *aX, PRInt32 *aY,
                                       PRInt32 *aWidth, PRInt32 *aHeight)
{
  NS_ENSURE_ARG_POINTER(aX);
  *aX = 0;
  NS_ENSURE_ARG_POINTER(aY);
  *aY = 0;
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Get x coordinate and width from treechildren element, get y coordinate and
  // height from tree cell.

  nsCOMPtr<nsIDOMElement> tcElm;
  mTree->GetTreeBody(getter_AddRefs(tcElm));
  nsCOMPtr<nsIDOMXULElement> tcXULElm(do_QueryInterface(tcElm));
  NS_ENSURE_STATE(tcXULElm);

  nsCOMPtr<nsIBoxObject> boxObj;
  tcXULElm->GetBoxObject(getter_AddRefs(boxObj));
  NS_ENSURE_STATE(boxObj);

  PRInt32 cellStartX, cellWidth;
  nsresult rv = mTree->GetCoordsForCellItem(mRow, mColumn, EmptyCString(),
                                            &cellStartX, aY,
                                            &cellWidth, aHeight);
  NS_ENSURE_SUCCESS(rv, rv);

  boxObj->GetWidth(aWidth);

  PRInt32 tcX = 0, tcY = 0;
  boxObj->GetScreenX(&tcX);
  boxObj->GetScreenY(&tcY);

  *aX = tcX;
  *aY += tcY;

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeitemAccessibleWrap::GetName(nsAString& aName)
{
  NS_ENSURE_STATE(mTree);
  nsCOMPtr<nsITreeColumns> cols;
  mTree->GetColumns(getter_AddRefs(cols));
  if (!cols) {
    return NS_OK;
  }
  nsCOMPtr<nsITreeColumn> column;
  cols->GetFirstColumn(getter_AddRefs(column));
  while (column) {
    nsAutoString colText;
    mTreeView->GetCellText(mRow, column, colText);
    aName += colText + NS_LITERAL_STRING("  ");
    nsCOMPtr<nsITreeColumn> nextColumn;
    column->GetNext(getter_AddRefs(nextColumn));
    column = nextColumn;
  }

  return NS_OK;
}

