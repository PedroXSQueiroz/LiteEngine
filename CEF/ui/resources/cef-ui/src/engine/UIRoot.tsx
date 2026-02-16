import { useState, useEffect } from 'react';

import 'bootstrap/dist/css/bootstrap.min.css';
import { Card, Container, Row, Col, Form, Button } from 'react-bootstrap';

import { getElements, setRenderCallback, UIElementDescriptor, sendToNative } from './uiStore';

function PanelComponent({ descriptor, allElements }: { descriptor: UIElementDescriptor; allElements: UIElementDescriptor[] }) {
  const children = allElements.filter(e => e.parentId === descriptor.id);
  const isRoot = descriptor.parentId === -1;

  const content = (
    <Container fluid>
      <Row>
        {children.map(child => (
          <Col key={child.id}>
            <UIElementRenderer descriptor={child} allElements={allElements} />
          </Col>
        ))}
      </Row>
    </Container>
  );
  console.log('RENDERIZANDO UM CARD')

  if (isRoot) {
    return <div data-ui-id={descriptor.id} style={{ background: 'transparent' }}>{content}</div>;
  }

  console.log('NÃO É ROOT')
  return (
    <Card data-ui-id={descriptor.id}>
      <Card.Body>
        {content}
      </Card.Body>
    </Card>
  );
}

function TextComponent({ descriptor }: { descriptor: UIElementDescriptor }) {
  return <h2 data-ui-h2={descriptor.id}>{descriptor.text ?? ''}</h2>;
}

function TextInputComponent({ descriptor }: { descriptor: UIElementDescriptor }) {
  const [value, setValue] = useState(descriptor.text ?? '');

  useEffect(() => {
    if (descriptor.text !== undefined) setValue(descriptor.text);
  }, [descriptor.text]);

  return (
    <Form.Group data-ui-id={descriptor.id}>
      {descriptor.label && <Form.Label>{descriptor.label}</Form.Label>}
      <Form.Control
        type="text"
        value={value}
        onChange={(e) => {
          setValue(e.target.value);
          sendToNative({ id: descriptor.id, type: 'textChange', text: e.target.value });
        }}
      />
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
      data-ui-id={descriptor.id}
      type="checkbox"
      label={descriptor.label ?? ''}
      checked={checked}
      onChange={(e) => {
        setChecked(e.target.checked);
        sendToNative({ id: descriptor.id, type: 'checkChange', checked: e.target.checked });
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
    <Form.Group data-ui-id={descriptor.id}>
      {descriptor.label && <Form.Label>{descriptor.label}</Form.Label>}
      <Form.Select
        value={selected}
        onChange={(e) => {
          setSelected(e.target.value);
          sendToNative({ id: descriptor.id, type: 'selectChange', selectedOption: e.target.value });
        }}
      >
        <option value="">Selecione...</option>
        {(descriptor.options ?? []).map(opt => (
          <option key={opt.key} value={opt.key}>{opt.label}</option>
        ))}
      </Form.Select>
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

function UIElementRenderer({ descriptor, allElements }: { descriptor: UIElementDescriptor; allElements: UIElementDescriptor[] }) {
  switch (descriptor.type) {
    case 'panel':    return <PanelComponent descriptor={descriptor} allElements={allElements} />;
    case 'text':     return <TextComponent descriptor={descriptor} />;
    case 'textInput':return <TextInputComponent descriptor={descriptor} />;
    case 'checkbox': return <CheckboxComponent descriptor={descriptor} />;
    case 'combobox': return <ComboBoxComponent descriptor={descriptor} />;
    case 'button':   return <ButtonComponent descriptor={descriptor} />;
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
