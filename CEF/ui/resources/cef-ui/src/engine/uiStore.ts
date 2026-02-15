export class UIElementDescriptor {
  public id: number = 0;
  public type: 'panel' | 'text' | 'textInput' | 'checkbox' | 'combobox' | 'button' = 'panel';
  public parentId: number = -1; // -1 = root
  public line: number = 0;
  public column: number = 0;
  public lineSpan: number = 0;
  public columnSpan: number = 0;
  // Props específicas por tipo
  text?: string;
  checked?: boolean;
  options?: { key: string; label: string }[];
  selectedOption?: string;
  label?: string;
};

let elements: UIElementDescriptor[] = [];
let renderCallback: (() => void) | null = null;

export function setRenderCallback(cb: () => void) {
  renderCallback = cb;
}

export function getElements(): UIElementDescriptor[] {
  return elements;
}

// Chamado pelo C++
function addElement(descriptor: UIElementDescriptor) {
  const idx = elements.findIndex(e => e.id === descriptor.id);
  if (idx >= 0) {
    elements[idx] = { ...elements[idx], ...descriptor };
  } else {
    elements.push(descriptor);
  }
  
  console.log('[CEF UI] ELEMENT ADDED')

  renderCallback?.();
}

function updateElement(id: number, props: Partial<UIElementDescriptor>) {
  const el = elements.find(e => e.id === id);
  if (el) {
    Object.assign(el, props);
    renderCallback?.();
  }
}

function removeElement(id: number) {
  elements = elements.filter(e => e.id !== id);
  renderCallback?.();
}

function clearElements() {
  elements = [];
  renderCallback?.();
}

// Enviar mensagem para o C++ via CefMessageRouter
export function sendToNative(data: object): Promise<string> {
  return new Promise((resolve, reject) => {
    if (!(window as any).cefQuery) {
      console.warn('[uiStore] cefQuery not available');
      reject('cefQuery not available');
      return;
    }
    (window as any).cefQuery({
      request: JSON.stringify(data),
      onSuccess: (response: string) => resolve(response),
      onFailure: (_errorCode: number, errorMessage: string) => reject(errorMessage),
    });
  });
}

// Expor no window para o C++ chamar
(window as any).liteUI = {
  addElement,
  updateElement,
  removeElement,
  clearElements,
};

