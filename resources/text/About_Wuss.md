# Welcome to Wuss

**Wuss** is a small window manager that lives in *DPTLib*. It draws into a plain framebuffer, so it runs anywhere from a desktop to RISC OS or a web browser. It's essentially a fanboy recreation of the RISC OS desktop.

Like RISC OS, it expects a three-button mouse. From left to right the buttons are `SELECT`, `MENU` and `ADJUST`.

## Windows

- Click a window's title bar to bring it to the front
- Drag a **title bar** with `SELECT` to move a window, or with `ADJUST` to move it without raising it
- Scroll to see content that doesn't fit

## Menus

Click `MENU` over a window to open its menu. Rows with an arrow lead to submenus; ticked rows show the current choice.

## Tasks

Every window belongs to a *task*: a single `handle` callback that draws its content and reacts to events. Wuss does the rest.

### Try it!

Open this window's menu and pick another **Sample**, **Font** or **Colours**, or change the **Spacing**.
