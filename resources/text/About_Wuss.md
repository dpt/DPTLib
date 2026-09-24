# Welcome to Wuss

**Wuss** is a small window manager from *DPTLib*. It draws into a plain framebuffer, so it runs anywhere from a desktop to RISC OS or a web browser.

## Windows

- Drag a **titlebar** to move a window
- Click a window to bring it to the front
- Scroll to see content that doesn't fit

## Menus

Click the *middle* mouse button over a window to open its menu. Rows with an arrow lead to submenus; ticked rows show the current choice.

## Tasks

Every window belongs to a *task*: a single `handle` callback that draws its content and reacts to events. Wuss does the rest.

### Try it

Open this window's menu and pick another **Sample**, **Font** or **Colours**.
