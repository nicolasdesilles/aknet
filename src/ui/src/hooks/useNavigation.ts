import { useState, useCallback } from "react";
import { type PanelId } from "@/types/navigation";

export function useNavigation(initialPanel: PanelId = "audio-levels") {
  const [activePanel, setActivePanel] = useState<PanelId>(initialPanel);

  const navigateTo = useCallback((panelId: PanelId) => {
    setActivePanel(panelId);
  }, []);

  return {
    activePanel,
    navigateTo,
  };
}
