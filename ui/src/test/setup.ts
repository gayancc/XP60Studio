// jsdom has no layout engine; Svelte's element-size bindings need a ResizeObserver to exist. Sizes stay 0, which the components treat as "not measured yet".
class NoopResizeObserver {
  observe(): void {}
  unobserve(): void {}
  disconnect(): void {}
}
if (typeof globalThis.ResizeObserver === 'undefined') globalThis.ResizeObserver = NoopResizeObserver as unknown as typeof ResizeObserver;
