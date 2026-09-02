import { useState, useEffect, useCallback, useRef } from 'react';
import { useAudioBackend } from './useAudioBackend';
import type { ParameterType, ParameterMap, ParameterValueType } from '../types/IAudioBackend';

type Parameter<T extends ParameterType> = ParameterMap[T];

// Every adapted parameter (slider, toggle, comboBox) exposes the same
// getValue/setValue shape; only the value type differs per kind.
function readCurrent<T extends ParameterType>(param: Parameter<T>): ParameterValueType[T] {
  return param.getValue() as ParameterValueType[T];
}

/**
 * Binding to a native (JUCE relay) parameter.
 *
 * The third return is a drag-state callback for continuous sliders: it
 * begins/ends the host automation gesture and ignores inbound valueChanged
 * echoes while the pointer is down. Without that, react-knob-headless's
 * "current value + this event's delta" math fights the native round-trip
 * and the knob sticks, then jumps. KnobControl already keeps a local value
 * during drag; this stops the parent state from snapping back under it.
 */
export function useParameter<T extends ParameterType>(
  identifier: string,
  type: T
): [ParameterValueType[T], (value: ParameterValueType[T]) => void, (dragging: boolean) => void] {
  const backend = useAudioBackend();
  const param = backend.getParameterState(identifier, type) as Parameter<T>;
  const [value, setValue] = useState<ParameterValueType[T]>(() => readCurrent(param));
  const draggingRef = useRef(false);
  const lastSentRef = useRef<ParameterValueType[T]>(value);

  const updateValue = useCallback(
    (newValue: ParameterValueType[T]) => {
      if (newValue === lastSentRef.current) return;
      lastSentRef.current = newValue;
      setValue(newValue);
      (param.setValue as (v: ParameterValueType[T]) => void)(newValue);
    },
    [param]
  );

  const onDragStateChange = useCallback(
    (dragging: boolean) => {
      draggingRef.current = dragging;
      const slider = param as ParameterMap['slider'];
      if (dragging) slider.sliderDragStarted?.();
      else slider.sliderDragEnded?.();
    },
    [param]
  );

  useEffect(() => {
    let listenerId: number | undefined;

    if (param.valueChangedEvent) {
      listenerId = param.valueChangedEvent.addListener((newValue) => {
        if (draggingRef.current) return;
        const next = newValue as ParameterValueType[T];
        lastSentRef.current = next;
        setValue(next);
      });
    }

    // Close the initial-sync race: the backend's reply to the frontend's
    // startup `requestInitialUpdate` can land before this listener exists (or
    // be dropped entirely while the page is still loading, which is frequent
    // on Windows WebView2), leaving the knob at its default. Re-read whatever
    // state already arrived, then ask the backend to send it again now that
    // we're subscribed.
    const current = readCurrent(param);
    lastSentRef.current = current;
    setValue(current);
    param.requestInitialUpdate?.();

    return () => {
      if (listenerId !== undefined && param.valueChangedEvent) {
        param.valueChangedEvent.removeListener(listenerId);
      }
    };
  }, [param, type]);

  return [value, updateValue, onDragStateChange];
}
