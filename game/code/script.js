(function () {
  'use strict';

  /* ========== 年份 ========== */
  var yearEl = document.getElementById('year');
  if (yearEl) yearEl.textContent = new Date().getFullYear();

  /* ========== 移动端菜单 ========== */
  var menuToggle = document.getElementById('menuToggle');
  var nav = document.getElementById('nav');

  if (menuToggle && nav) {
    menuToggle.addEventListener('click', function () {
      var open = nav.classList.toggle('is-open');
      menuToggle.setAttribute('aria-expanded', open ? 'true' : 'false');
    });

    nav.querySelectorAll('a').forEach(function (link) {
      link.addEventListener('click', function () {
        nav.classList.remove('is-open');
        menuToggle.setAttribute('aria-expanded', 'false');
      });
    });
  }

  /* ========== 生成地面草方块 ========== */
  var ground = document.getElementById('ground');

  if (ground) {
    var COUNT = 26;

    for (var i = 0; i < COUNT; i++) {
      var block = document.createElement('div');
      block.className = 'block';
      ground.appendChild(block);
    }

    /* 点击挖掘：方块消失，1.5 秒后恢复 */
    ground.addEventListener('click', function (e) {
      var block = e.target.closest('.block');
      if (!block || block.classList.contains('is-mined')) return;

      block.classList.add('is-mined');

      setTimeout(function () {
        block.classList.remove('is-mined');
      }, 1500);
    });
  }

  /* ========== 数字滚动 ========== */
  var counters = document.querySelectorAll('.stat__num span[data-count]');

  function animateCount(el) {
    var target = parseInt(el.getAttribute('data-count'), 10) || 0;
    var duration = 1500;
    var startTime = performance.now();

    function step(now) {
      var progress = Math.min((now - startTime) / duration, 1);
      var ease = 1 - Math.pow(1 - progress, 3);
      el.textContent = Math.floor(ease * target).toLocaleString('en-US');

      if (progress < 1) {
        requestAnimationFrame(step);
      } else {
        el.textContent = target.toLocaleString('en-US');
      }
    }

    requestAnimationFrame(step);
  }

  if (counters.length && 'IntersectionObserver' in window) {
    var counterObserver = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) {
          animateCount(entry.target);
          counterObserver.unobserve(entry.target);
        }
      });
    }, { threshold: 0.5 });

    counters.forEach(function (c) { counterObserver.observe(c); });
  } else {
    counters.forEach(function (c) {
      c.textContent = parseInt(c.getAttribute('data-count'), 10).toLocaleString('en-US');
    });
  }

  /* ========== 导航高亮 ========== */
  var navLinks = document.querySelectorAll('.nav__link');
  var sections = document.querySelectorAll('main section[id]');

  if (navLinks.length && sections.length && 'IntersectionObserver' in window) {
    var linkMap = {};

    navLinks.forEach(function (link) {
      var href = link.getAttribute('href');
      if (href && href.charAt(0) === '#') {
        linkMap[href.slice(1)] = link;
      }
    });

    var sectionObserver = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting && linkMap[entry.target.id]) {
          navLinks.forEach(function (l) { l.classList.remove('is-active'); });
          linkMap[entry.target.id].classList.add('is-active');
        }
      });
    }, { rootMargin: '-45% 0px -50% 0px' });

    sections.forEach(function (s) { sectionObserver.observe(s); });
  }

  /* ========== 返回顶部 ========== */
  var toTop = document.getElementById('toTop');

  if (toTop) {
    window.addEventListener('scroll', function () {
      toTop.classList.toggle('is-show', window.scrollY > 500);
    }, { passive: true });

    toTop.addEventListener('click', function () {
      window.scrollTo({ top: 0, behavior: 'smooth' });
    });
  }

})();