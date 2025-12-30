import { exposed } from "@saucer-dev/types";

import { Button } from "@/components/ui/button";

function handleClick() {
  exposed<void, [void]>("log_test_msg")();
}

function App() {
  return (
    <div className="flex min-h-svh flex-col items-center justify-center">
      <Button onClick={handleClick}>Click me</Button>
    </div>
  );
}

export default App;
