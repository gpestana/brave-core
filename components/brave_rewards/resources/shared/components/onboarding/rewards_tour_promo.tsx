/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import { CloseIcon } from '../icons/close_icon'

import * as style from './rewards_tour_promo.style'

interface Props {
  onClose: () => void
  onTakeTour: () => void
}

export function RewardsTourPromo (props: Props) {
  return (
    <style.root>
      <style.close>
        <button onClick={props.onClose}><CloseIcon /></button>
      </style.close>
      <style.header>
        Need a refresher on Rewards?
      </style.header>
      <style.text>
        Take a quick tour to brush up on how it works to go down in history as
        a Rewards rockstar!
      </style.text>
      <style.action>
        <button onClick={props.onTakeTour}>Take a quick tour</button>
      </style.action>
    </style.root>
  )
}
