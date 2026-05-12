import { render } from 'preact';
import './stores/theme';        // applies data-theme before first render
import { App } from './app';
import './index.css';

render(<App />, document.getElementById('app'));
