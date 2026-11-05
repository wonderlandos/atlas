// SPDX-License-Identifier: BSD-3-Clause
// Atlas first run: four screens, one primary action each, keyboard first.
// No network, no innerHTML (the page runs under Trusted Types), no inline handlers.
'use strict';

(function () {
  const host = window.atlasFirstRun;
  const screens = Array.from(document.querySelectorAll('.screen'));
  const pips = Array.from(document.querySelectorAll('.pip'));
  const label = document.getElementById('step-label');
  const frame = document.getElementById('frame');
  const finishButton = document.getElementById('finish');
  const defaultCard = document.getElementById('default-card');
  const makeDefault = document.getElementById('make-default');
  const baseTitle = document.title;
  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)');
  let current = 0;
  let done = false;

  function reduceMotion() {
    return reduced.matches || document.documentElement.classList.contains('reduce-motion');
  }

  // Rows stagger 30 ms apart; the spread is capped at 300 ms (motion.md).
  function numberRows(screen) {
    screen.querySelectorAll('.row, .key-row').forEach((row, i) => {
      row.style.setProperty('--i', String(Math.min(i, 10)));
    });
  }

  function show(index, focus) {
    current = Math.max(0, Math.min(index, screens.length - 1));
    screens.forEach((s, i) => {
      s.hidden = i !== current;
      s.classList.remove('is-entering');
    });
    const screen = screens[current];
    if (!reduceMotion()) {
      void screen.offsetWidth; // restart the entrance
      screen.classList.add('is-entering');
    }
    pips.forEach((p, i) => {
      p.classList.toggle('is-done', i < current);
      p.classList.toggle('is-current', i === current);
    });
    const name = screen.dataset.title;
    label.textContent = 'Step ' + (current + 1) + ' of ' + screens.length + ': ' + name;
    document.title = current === 0 ? baseTitle : name + ' - ' + baseTitle;
    if (focus) {
      const h = screen.querySelector('h1');
      if (h) h.focus();
    }
  }

  function choices(skipped) {
    const privacy = {};
    document.querySelectorAll('.switch[data-pref]').forEach((s) => {
      privacy[s.dataset.pref] = s.getAttribute('aria-checked') === 'true';
    });
    const start = document.querySelector('input[name="start"]:checked');
    return {
      skipped: skipped,
      privacy: privacy,
      start: skipped || !start ? 'clean' : start.value,
      makeDefault: !skipped && !defaultCard.hidden && makeDefault.getAttribute('aria-checked') === 'true',
    };
  }

  function finish(skipped) {
    if (done) return;
    done = true;
    const result = choices(skipped);
    let handed = false;
    const hand = () => {
      if (handed) return;
      handed = true;
      host.finish(result);
    };
    if (reduceMotion() || host.preview) {
      hand();
      return;
    }
    // The card fades out on fast; the engine then opens the next page.
    frame.classList.add('is-leaving');
    frame.addEventListener('animationend', (e) => { if (e.target === frame) hand(); });
    window.setTimeout(hand, 400);
  }

  function syncStartLabel() {
    const start = document.querySelector('input[name="start"]:checked');
    finishButton.textContent = start && start.value === 'import' ? 'Choose a browser…' : 'Start browsing';
  }

  document.addEventListener('click', (e) => {
    const t = e.target.closest('button');
    if (!t) return;
    if (t.classList.contains('switch')) {
      t.setAttribute('aria-checked', t.getAttribute('aria-checked') === 'true' ? 'false' : 'true');
      return;
    }
    switch (t.dataset.action) {
      case 'next': show(current + 1, true); break;
      case 'back': show(current - 1, true); break;
      case 'skip': finish(true); break;
      case 'finish': finish(false); break;
      default: break;
    }
  });

  document.addEventListener('change', (e) => {
    if (e.target.name === 'start') syncStartLabel();
  });

  screens.forEach(numberRows);

  host.getState().then((state) => {
    if (state.reducedMotion === true) document.documentElement.classList.add('reduce-motion');
    document.querySelectorAll('.switch[data-pref]').forEach((s) => {
      const v = state.privacy && state.privacy[s.dataset.pref];
      if (typeof v === 'boolean') s.setAttribute('aria-checked', v ? 'true' : 'false');
    });
    defaultCard.hidden = state.isDefault || !state.canBeDefault;
  }).catch(() => {
    defaultCard.hidden = true;
  }).finally(() => {
    syncStartLabel();
    show(0, false);
  });
})();
