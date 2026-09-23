import { useState, useEffect, useRef } from 'react';
import type { ReactElement } from 'react';

import 'bootstrap/dist/css/bootstrap.min.css';
import { Card, Row, Col, Form, Button } from 'react-bootstrap';

import { getElements, setRenderCallback, UIElementDescriptor, type UITreeNodeDescriptor, sendToNative } from './uiStore';
import './UIRoot.css'

function PanelComponent({ descriptor, allElements }: { descriptor: UIElementDescriptor; allElements: UIElementDescriptor[] }) {
  const children = allElements.filter(e => e.parentId === descriptor.id);
  const isRoot = descriptor.parentId === -1;

  const totalCols = children.reduce((max, child) => Math.max(max, child.column + (child.columnSpan || 1)), 1);

  const content = (
    <div style={{
      display: 'grid',
      gridTemplateColumns: `repeat(${totalCols}, 1fr)`,
      gap: '6px',
      width: '100%'
    }}>
      {children.map(child => (
        <div
          key={child.id}
          style={{
            gridColumn: `${child.column + 1} / span ${child.columnSpan || 1}`,
            gridRow: `${child.line + 1} / span ${child.lineSpan || 1}`,
          }}
        >
          <UIElementRenderer descriptor={child} allElements={allElements} />
        </div>
      ))}
    </div>
  );
  
  if (isRoot) {
    return <div data-ui-id={descriptor.id} style={{ background: 'transparent' }}>{content}</div>;
  }

  return (
    <Card data-ui-id={descriptor.id}>
      <Card.Body>
        {content}
      </Card.Body>
    </Card>
  );
}

function TextComponent({ descriptor }: { descriptor: UIElementDescriptor }) {
  return <span data-ui-h2={descriptor.id}>{descriptor.text ?? ''}</span>;
}

function TextInputComponent({ descriptor }: { descriptor: UIElementDescriptor }) {
  const [value, setValue] = useState(descriptor.text ?? '');
  const debounceRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    if (descriptor.text !== undefined) setValue(descriptor.text);
  }, [descriptor.text]);

  const commitValue = (val: string) => {
    console.log(`campo editado:${val}`);
    sendToNative({ id: descriptor.id, type: 'changeValue', value: val });
  };

  return (
    <Form.Group as={Row} data-ui-id={descriptor.id} style={{width:'100%'}} className="align-items-center">
      {descriptor.label && <Form.Label column sm="auto" className='input-label'>{descriptor.label}</Form.Label>}
      <Col>
        <Form.Control
          type="text"
          value={value}
          style={{width:'100%'}}
          onChange={(e) => {
            setValue(e.target.value);
            if (debounceRef.current) clearTimeout(debounceRef.current);
            debounceRef.current = setTimeout(() => commitValue(e.target.value), 400);
          }}
        />
      </Col>
    </Form.Group>
  );
}

function CheckboxComponent({ descriptor }: { descriptor: UIElementDescriptor }) {
  const [checked, setChecked] = useState(descriptor.checked ?? false);

  useEffect(() => {
    if (descriptor.checked !== undefined) setChecked(descriptor.checked);
  }, [descriptor.checked]);

  return (
    <Form.Check
      as={Row}
      data-ui-id={descriptor.id}
      type="checkbox"
      label={descriptor.label ?? ''}
      checked={checked}
      onChange={(e) => {
        setChecked(e.target.checked);
        sendToNative({ id: descriptor.id, type: 'changeValue', value: e.target.checked.toString() });
      }}
    />
  );
}

function ComboBoxComponent({ descriptor }: { descriptor: UIElementDescriptor }) {
  const [selected, setSelected] = useState(descriptor.selectedOption ?? '');

  useEffect(() => {
    if (descriptor.selectedOption !== undefined) setSelected(descriptor.selectedOption);
  }, [descriptor.selectedOption]);

  return (
    <Form.Group as={Row} data-ui-id={descriptor.id}>
      {descriptor.label && <Form.Label>{descriptor.label}</Form.Label>}
      <Col>
        <Form.Select
          value={selected}
          onChange={(e) => {
            setSelected(e.target.value);
            sendToNative({ id: descriptor.id, type: 'changeValue', value: e.target.value });
          }}
        >
          <option value="">Selecione...</option>
          {(descriptor.options ?? []).map(opt => (
            <option key={opt.key} value={opt.key}>{opt.label}</option>
          ))}
        </Form.Select>
      </Col>
    </Form.Group>
  );
}

function ButtonComponent({ descriptor }: { descriptor: UIElementDescriptor }) {
  return (
    <Button
      data-ui-id={descriptor.id}
      variant="primary"
      onClick={() => {
        sendToNative({ id: descriptor.id, type: 'click' });
      }}
    >
      {descriptor.label ?? descriptor.text ?? 'Button'}
    </Button>
  );
}

function TreeComponent({ descriptor }: { descriptor: UIElementDescriptor }) {
  // Expansão e seleção são estado local do React; nós começam recolhidos.
  const [expanded, setExpanded] = useState<Record<number, boolean>>({});
  const [selectedId, setSelectedId] = useState<number | null>(null);

  const renderNodes = (nodes: UITreeNodeDescriptor[], depth: number): ReactElement[] =>
    nodes.flatMap(node => {
      const hasChildren = node.children.length > 0;
      const isExpanded = !!expanded[node.id];
      const isSelected = node.id === selectedId;

      const row = (
        <div
          key={node.id}
          className={`tree-row${isSelected ? ' selected' : ''}`}
          style={{ paddingLeft: `${10 + depth * 16}px` }}
          onClick={() => {
            setSelectedId(node.id);
            sendToNative({ id: descriptor.id, type: 'click', value: node.id.toString() });
          }}
        >
          {hasChildren
            ? (
              <span
                className="tree-caret"
                onClick={(e) => {
                  e.stopPropagation();
                  setExpanded(prev => ({ ...prev, [node.id]: !prev[node.id] }));
                }}
              >
                {isExpanded ? '▾' : '▸'}
              </span>
            )
            : <span className="tree-caret" />}
          <span className="tree-label">{node.label}</span>
        </div>
      );

      return hasChildren && isExpanded
        ? [row, ...renderNodes(node.children, depth + 1)]
        : [row];
    });

  return (
    <div data-ui-id={descriptor.id} className="tree">
      {renderNodes(descriptor.nodes ?? [], 0)}
    </div>
  );
}

function UIElementRenderer({ descriptor, allElements }: { descriptor: UIElementDescriptor; allElements: UIElementDescriptor[] }) {
  switch (descriptor.type) {
    case 'panel':    return <PanelComponent descriptor={descriptor} allElements={allElements} />;
    case 'text':     return <TextComponent descriptor={descriptor} />;
    case 'textInput':return <TextInputComponent descriptor={descriptor} />;
    case 'checkbox': return <CheckboxComponent descriptor={descriptor} />;
    case 'combobox': return <ComboBoxComponent descriptor={descriptor} />;
    case 'button':   return <ButtonComponent descriptor={descriptor} />;
    case 'tree':     return <TreeComponent descriptor={descriptor} />;
    default:         return null;
  }
}

export default function UIRoot() {
  const [, setVersion] = useState(0);
  const [isUiReady, setUiReady] =  useState(false);

  useEffect(() => {
    setRenderCallback(() => setVersion(v => v + 1));
    return () => setRenderCallback(() => { /*DOES NOTHING, WHY?*/ });
  }, []);

  useEffect(() => {
    if(!isUiReady)
    {
      console.log('UI READY EVENT SENT');
      
      sendToNative({
        "event": "ui_ready"
      });

      setUiReady(true);
    }
  });

  const allElements = getElements();
  const roots = allElements.filter(e => e.parentId === -1);

  console.log('[CEF UI] ROOT DRAWED')

  return (
    <>
      {roots.map(el => (
        <UIElementRenderer key={el.id} descriptor={el} allElements={allElements} />
      ))}
    </>
  );
}
