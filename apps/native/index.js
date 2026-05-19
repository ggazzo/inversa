// Expo entry point. `registerRootComponent` wraps `App` with the
// AppRegistry stuff RN expects, and also handles dev-client niceties.
//
// `react-native-reanimated` and `react-native-gesture-handler` must
// load before any other module imports them — gesture-handler in
// particular needs to install its global JSI bindings before the JS
// React tree touches anything that listens for gestures. Importing
// them as side-effects at the very top guarantees they initialise
// first. It also surfaces the real init error here (instead of
// getting laundered through @shopify/react-native-skia's optional-
// dependency proxy) if either one fails to set up.
import 'react-native-gesture-handler';
import 'react-native-reanimated';

import { registerRootComponent } from 'expo';
import App from './App';

registerRootComponent(App);
