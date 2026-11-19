(() => {
  const canHover = matchMedia('(hover: hover) and (pointer: fine)');
  document.querySelectorAll('.ime-download').forEach(box => {
    if (box.dataset.imeBound) return;
    box.dataset.imeBound = '1';
    const trigger = box.querySelector('summary');
    const panel = box.querySelector('.ime-download-body');
    if (!trigger || !panel) return;
    let pinned = false;
    let suppressHover = false;
    let closeTimer;
    let frame = 0;

    function place() {
      if (!box.open) return;
      box.style.setProperty('--ime-panel-x', '0px');
      const anchor = trigger.getBoundingClientRect();
      const below = innerHeight - anchor.bottom;
      const above = anchor.top;
      const upward = below < Math.min(panel.scrollHeight, 420) + 20 && above > below;
      box.dataset.placement = upward ? 'above' : 'below';
      const available = upward ? above : below;
      const height = Math.min(Math.max(150, available - 24), Math.max(150, innerHeight - 32));
      box.style.setProperty('--ime-panel-height', `${height}px`);
      const rect = panel.getBoundingClientRect();
      const shift = rect.right > innerWidth - 12
        ? Math.max(12 - rect.left, innerWidth - 12 - rect.right)
        : (rect.left < 12 ? 12 - rect.left : 0);
      if (shift) box.style.setProperty('--ime-panel-x', `${Math.round(shift)}px`);
    }

    function schedulePlace() {
      if (!box.open || frame) return;
      frame = requestAnimationFrame(() => {
        frame = 0;
        place();
      });
    }

    function open() {
      clearTimeout(closeTimer);
      box.open = true;
      trigger.setAttribute('aria-expanded', 'true');
      place();
    }

    function close() {
      clearTimeout(closeTimer);
      box.open = false;
      trigger.setAttribute('aria-expanded', 'false');
    }

    box.open = false;
    trigger.setAttribute('aria-expanded', 'false');
    box.addEventListener('pointerenter', event => {
      if (event.pointerType === 'mouse' && canHover.matches && !suppressHover) open();
    });
    box.addEventListener('pointerleave', event => {
      if (event.pointerType !== 'mouse') return;
      suppressHover = false;
      if (!pinned) closeTimer = setTimeout(() => {
        if (!box.contains(document.activeElement)) close();
      }, 180);
    });
    panel.addEventListener('pointerenter', () => clearTimeout(closeTimer));
    trigger.addEventListener('click', event => {
      event.preventDefault();
      if (pinned) {
        pinned = false;
        suppressHover = true;
        close();
      } else {
        pinned = true;
        open();
      }
    });
    box.addEventListener('focusout', () => {
      setTimeout(() => {
        if (!pinned && !box.contains(document.activeElement) && !box.matches(':hover')) close();
      }, 0);
    });
    document.addEventListener('pointerdown', event => {
      if (event.target instanceof Node && !box.contains(event.target)) {
        pinned = false;
        close();
      }
    });
    document.addEventListener('keydown', event => {
      if (event.key === 'Escape' && box.open) {
        pinned = false;
        suppressHover = box.matches(':hover');
        close();
        trigger.focus({preventScroll: true});
        event.preventDefault();
      }
    });
    box.addEventListener('toggle', () => {
      trigger.setAttribute('aria-expanded', String(box.open));
      if (box.open) place();
    });
    addEventListener('resize', schedulePlace, {passive: true});
    addEventListener('scroll', schedulePlace, {passive: true});
  });
})();
