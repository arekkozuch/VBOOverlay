import type { FlappedEarApi } from '../shared/ipc';
declare global {
  interface Window {
    flappedEar: FlappedEarApi;
  }
}
export {};
