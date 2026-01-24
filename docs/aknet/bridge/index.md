---
generator: doxide
---


# bridge

Event bridge module for C++ to JavaScript communication.

This module provides a bridge between the C++ backend and the JavaScript/React frontend,
forwarding events from various modules (like StartupManager) to the webview as DOM CustomEvents.

The bridge uses the saucer webview library to execute JavaScript in the embedded browser.
Events are serialized to JSON and dispatched via `window.dispatchEvent()`.

## Design Goals

- **Decoupling**: C++ modules don't need to know about the UI implementation
- **Type Safety**: Events are serialized using the existing JSON serialization infrastructure
- **Testability**: Template-based webview interface allows mocking for tests
- **Lifecycle Management**: Automatic cleanup of event listeners on disconnect/destruction

## Event Flow

```
StartupManager.events() → EventBridge → webview.execute(js) → window.dispatchEvent()
```

## Frontend Integration

The frontend listens for events using standard DOM event listeners:

```typescript
window.addEventListener('startup:progress', (e: CustomEvent) => {
    const progress = e.detail as SequenceProgress;
    // Update UI...
});
```


:material-eye-outline: **See**
:    startup::StartupManager for the event source

:material-eye-outline: **See**
:    startup::json for JSON serialization of event payloads


## Types

| Name | Description |
| ---- | ----------- |
| [EventBridge](EventBridge.md) | Bridge for forwarding C++ events to the webview as JavaScript CustomEvents. |

