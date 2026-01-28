import { type LucideIcon } from "lucide-react";

interface SidebarItemProps {
  icon: LucideIcon;
  isActive: boolean;
  onClick: () => void;
}

export function SidebarItem({
  icon: Icon,
  isActive,
  onClick,
}: SidebarItemProps) {
  return (
    <div>
      <button
        onClick={onClick}
        className={`
              relative w-full h-14 flex items-center justify-center
              transition-all duration-200
              ${
                isActive
                  ? "bg-accent/15 text-accent"
                  : "text-muted-foreground hover:bg-accent/5 hover:text-foreground"
              }
            `}
      >
        {/* Active indicator bar */}
        {isActive && (
          <div className="absolute left-0 top-0 bottom-0 w-0.5 bg-accent" />
        )}

        <Icon className="h-5 w-5" />
      </button>
    </div>
  );
}
