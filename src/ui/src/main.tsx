import { StrictMode } from "react";
import { createRoot } from "react-dom/client";

import "@fontsource/geist-sans/400.css"; // Regular weight
import "@fontsource/geist-sans/500.css"; // Medium weight
import "@fontsource/geist-sans/600.css"; // Semibold weight
import "@fontsource/geist-sans/700.css"; // Bold weight
import "@fontsource/geist-mono/400.css"; // Mono regular
import "@fontsource/geist-mono/500.css"; // Mono medium

import "./index.css";
import App from "./App.tsx";
import { Toaster } from "@/components/ui/sonner";

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
    <Toaster position="bottom-right" />
  </StrictMode>,
);
