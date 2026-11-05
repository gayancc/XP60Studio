<script lang="ts">
  // The status readout in the header: a recessed well, like the displays in FL Studio's toolbar. Three lines: where you are and
  // the sound's state; the name of what is open (or, while a control is under the pointer, that control's value in the accent
  // colour); and the instrument link. It reads out; it does not take input.

  interface Props {
    /** Top row, left: where you are ("PATCH"). */
    title: string;
    /** Top row, right: the state of the sound ("EDITED"). */
    badge: string;
    /** The colour of the state's dot and word. */
    badgeColor?: string;
    /** The big line: the name of what is open, or the value under the pointer. */
    main: string;
    /** A small chip beside the big line ("#012"). */
    tag?: string;
    /** The big line is a live value from a control under the pointer: it takes the accent colour. */
    hovering?: boolean;
    /** The bottom row: the instrument link, or the name of the parameter under the pointer. */
    status: string;
    /** The instrument is connected: the link light is green. */
    linked?: boolean;
    /** Bottom row, right. */
    status2?: string;
    /** The state pulses while something is waiting to be sent. */
    busy?: boolean;
    width?: number;
    height?: number;
  }

  let {
    title,
    badge,
    badgeColor = 'var(--text-dim)',
    main,
    tag = '',
    hovering = false,
    status,
    linked = false,
    status2 = '',
    busy = false,
    width = 284,
    height = 52,
  }: Props = $props();
</script>

<div class="ro" role="status" aria-label="Status: {title}, {badge}, {main}. {status}" style:width="{width}px" style:height="{height}px">
  <div class="top">
    <span class="title">{title}</span>
    {#if badge}
      <span class="badge" class:busy style:--c={badgeColor}><i></i>{badge}</span>
    {/if}
  </div>
  <div class="mid">
    <span class="main" class:live={hovering} title={main}>{main}</span>
    {#if tag}<span class="tag">{tag}</span>{/if}
  </div>
  <div class="low">
    <span class="link" class:on={linked && !hovering}></span>
    <span class="status">{status}</span>
    {#if status2}<span class="tx">{status2}</span>{/if}
  </div>
</div>

<style>
  .ro {
    flex: none;
    box-sizing: border-box;
    display: flex;
    flex-direction: column;
    justify-content: space-between;
    padding: 5px 9px 5px;
    border: 1px solid var(--edge);
    border-radius: var(--radius-lg);
    background: linear-gradient(var(--track), color-mix(in srgb, var(--track) 82%, #000));
    box-shadow:
      inset 0 1px 3px rgba(0, 0, 0, 0.5),
      0 1px 0 rgba(255, 255, 255, 0.06);
    font-family: var(--font-caps);
    overflow: hidden;
  }
  .top,
  .mid,
  .low {
    display: flex;
    align-items: center;
    gap: 8px;
    min-width: 0;
  }
  .top {
    justify-content: space-between;
    font-size: 9.5px;
    letter-spacing: 0.09em;
    text-transform: uppercase;
    line-height: 1;
  }
  .title {
    color: var(--text-dim);
    white-space: nowrap;
  }
  .badge {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    color: var(--c);
    font-weight: 700;
    white-space: nowrap;
  }
  .badge i {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--c);
    box-shadow: 0 0 4px color-mix(in srgb, var(--c) 70%, transparent);
  }
  .badge.busy i {
    animation: pulse 0.9s ease-in-out infinite;
  }
  .mid {
    justify-content: space-between;
  }
  .main {
    min-width: 0;
    flex: 1;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    color: var(--text);
    font-size: 17px;
    font-weight: 600;
    letter-spacing: 0.03em;
    line-height: 1.15;
  }
  .main.live {
    color: var(--accent);
  }
  .tag {
    flex: none;
    padding: 1px 6px;
    border: 1px solid var(--accent-soft);
    border-radius: var(--radius);
    color: var(--accent);
    font: 600 10px var(--font-mono);
    letter-spacing: 0.04em;
  }
  .low {
    font-size: 9.5px;
    letter-spacing: 0.06em;
    text-transform: uppercase;
    line-height: 1;
    color: var(--text-dim);
  }
  .link {
    flex: none;
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--edge-light);
    box-shadow: inset 0 0 0 1px rgba(0, 0, 0, 0.4);
  }
  .link.on {
    background: var(--ok);
    box-shadow: 0 0 4px var(--ok);
  }
  .status {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .tx {
    flex: none;
    color: var(--accent);
    font-weight: 700;
  }
  @keyframes pulse {
    50% {
      opacity: 0.25;
    }
  }
  @media (prefers-reduced-motion: reduce) {
    .badge.busy i {
      animation: none;
    }
  }
</style>
