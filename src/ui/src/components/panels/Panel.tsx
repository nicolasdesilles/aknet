interface PanelProps {
  children: React.ReactNode;
  title?: string;
}

export function Panel({ children, title }: PanelProps) {
  return (
    <div className="w-full h-full flex flex-col">
      {title && (
        <header className="flex-shrink-0 h-14 px-6 border-b border-border flex items-center">
          <h2 className="text-lg font-semibold">{title}</h2>
        </header>
      )}
      <div className="flex-1 overflow-auto p-6">{children}</div>
    </div>
  );
}
