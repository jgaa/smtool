# Edit Dialogs

For simple lookup-style tables, use a separate edit dialog rather than inline editing inside the list. The list dialog should focus on browsing, reordering, and launching add/edit/delete actions.

## Expected Pattern

Use one management dialog plus one add/edit dialog.

The management dialog should:

* show the existing rows as cards or rows in a list
* provide a clear `Add New` button near the top of the dialog, above the list content
* provide delete affordance directly on each row, typically a delete/trash icon button
* launch editing in a separate dialog instead of embedding editors into the list

The add/edit dialog should:

* keep Save disabled until required fields are valid
* mark invalid fields visually in the form, for example with a red border or text color
* allow the user to see and fix problems without closing the dialog
* show save errors in the dialog itself, including database constraint failures

If a save or delete fails because of validation or a database constraint, explain the real reason to the user and do not close the dialog automatically. Prefer a direct explanation such as “cannot delete because existing publications still reference this channel” instead of a vague generic failure message.

## Sorting

For tables with a sort column such as `sort_order`, treat ordering as explicit user-managed state.

* allow drag reordering in the management dialog
* use a common drag-handle icon rather than text like `Move`
* persist the full order on drop or explicit save
* make sure any item can move to the first or last position
* define where new items go; do not rely on incidental database order

If the product needs a default insertion rule, state it explicitly in the feature spec. Current channel behavior inserts new items at the top and shifts existing items down.

## Interaction Notes

Use consistent triggers for row actions across simple-table dialogs unless a feature spec says otherwise.

Current channel behavior is:

* drag handle for reordering
* long-press on a row to open the edit dialog
* delete icon on each row

Future simple-table dialogs should follow the same pattern when it fits the platform and interaction model.
