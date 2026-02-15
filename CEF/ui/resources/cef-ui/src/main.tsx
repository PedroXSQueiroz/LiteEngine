import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import 'bootstrap/dist/css/bootstrap.min.css'
import './index.css'
import UIRoot from './engine/UIRoot'
import './engine/uiStore' // registra window.liteUI

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <UIRoot />
  </StrictMode>
)
