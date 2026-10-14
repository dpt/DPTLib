# CONTEXT.md

Domain glossary for DPTLib. Use these terms in code, docs and design discussion.

## wuss

**Menu session**: the state and behaviour of one open menu interaction in wuss: the chain of open menus and submenus, the owning task, the bottom gap, the `eat_up` flag that swallows the tail of the press that opened the menu, outside-click dismissal, key handling while open, and abandonment when a window closes or the owning task is destroyed. Owned by the menu layer (`libraries/wuss/menu/`). Core reaches it only through a few entry points: press, key, window-closed, task-destroyed. State is opaque to core.
