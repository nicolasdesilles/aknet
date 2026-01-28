import * as Icons from "lucide-react";
import { SidebarItem } from "./SidebarItem";
import { PANELS, type PanelId } from "@/types/navigation";
import type { LucideIcon } from "lucide-react";

interface SidebarProps {
  activePanel: PanelId;
  onNavigate: (panelId: PanelId) => void;
}

export function Sidebar({ activePanel, onNavigate }: SidebarProps) {
  return (
    <aside className="w-14 h-full bg-sidebar border-r border-sidebar-border flex flex-col">
      {/* Navigation items */}
      <nav className="flex-1 py-2">
        {PANELS.map((panel) => {
          // Dynamically get the icon component
          const IconComponent = Icons[
            panel.icon as keyof typeof Icons
          ] as LucideIcon;

          return (
            <SidebarItem
              key={panel.id}
              icon={IconComponent}
              isActive={activePanel === panel.id}
              onClick={() => onNavigate(panel.id)}
            />
          );
        })}
      </nav>
    </aside>
  );
}
