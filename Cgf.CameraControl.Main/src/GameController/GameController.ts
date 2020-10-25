import { ImageConnectionFactory } from '../ImageConnection/ImageConnectionFactory';
import { State } from '../State';
import { ControllerConfig } from './ControllerConfig';
import { AtemConnection } from '../AtemConnection/AtemConnection';
import { IImageConnection } from '../ImageConnection/IImageConnection';
import { GamepadFactory } from './Gamepads/GamepadFactory';
import { AlternateInputChangeDirection, IGamePad, InputChangeDirection } from './Gamepads/IGamePad';
import { number } from 'yargs';

class InternalImageConnection {
    constructor(
        public readonly connection: IImageConnection,
        public readonly connectionChangeDefinition: {
            [key in InputChangeDirection]: number;
        },
        public readonly altConnectionChangeDefinition?: {
            [key in InputChangeDirection]: number;
        }
    ) {}
}

export class GameController {
    private state: State;
    private imageConnections: Array<InternalImageConnection> = [];
    private currentCameraConnection?: InternalImageConnection;
    private AtemMixEffectBlock: number;
    private pad: IGamePad;

    constructor(config: ControllerConfig, private atem: AtemConnection) {
        this.state = new State();

        this.pad = GamepadFactory.getGamepad(config);
        this.connectGamepad(config);

        config.ImageConnections.forEach((c) => {
            this.imageConnections.push(
                new InternalImageConnection(
                    ImageConnectionFactory.GetImageConnection(c),
                    c.ConnectionChangeDefinition,
                    c.AltConnectionChangeDefinition
                )
            );
        });

        this.AtemMixEffectBlock = config.AtemMixEffectBlock;
        this.atem
            .previewStateUpdateEmitterGet(this.AtemMixEffectBlock)
            .on('previewUpdate', (preview: number, isOnAir: boolean) =>
                this.selectedConnectionChanged(preview, isOnAir)
            );
    }

    private connectGamepad(config: ControllerConfig) {
        this.pad.keypadEvents$.on('pan', (pan) => {
            this.state.pan = pan;
            this.currentCameraConnection?.connection.setState(this.state);
        });

        this.pad.keypadEvents$.on('tilt', (tilt) => {
            this.state.tilt = tilt;
            this.currentCameraConnection?.connection.setState(this.state);
        });

        this.pad.keypadEvents$.on('zoom', (zoom) => {
            this.state.zoom = zoom;
            this.currentCameraConnection?.connection.setState(this.state);
        });

        this.pad.keypadEvents$.on('focus', (focus) => {
            this.state.focus = focus;
            this.currentCameraConnection?.connection.setState(this.state);
        });

        this.pad.keypadEvents$.on('inputChange', (direction, directionAlt) => {
            this.changeConnection(direction, directionAlt);
        });

        this.pad.keypadEvents$.on('cut', () => {
            this.atem.cut(config.AtemMixEffectBlock);
        });

        this.pad.keypadEvents$.on('auto', () => {
            this.atem.auto(config.AtemMixEffectBlock);
        });

        this.pad.keypadEvents$.on('keyToggle', (index) => {
            if (config.AtemToggleKeyIndexes[index] !== undefined) {
                this.atem.toggleKey(config.AtemToggleKeyIndexes[index], config.AtemMixEffectBlock);
            }
        });
    }

    changeConnection(direction: InputChangeDirection, directionAlt: AlternateInputChangeDirection) {
        let next: number | undefined = undefined;
        switch (directionAlt) {
            case AlternateInputChangeDirection.alt:
                if (this.currentCameraConnection) {
                    if (this.currentCameraConnection.altConnectionChangeDefinition) {
                        next = this.currentCameraConnection.altConnectionChangeDefinition[direction];
                    } else {
                        next = this.currentCameraConnection.connectionChangeDefinition[direction];
                    }
                }
                break;
            case AlternateInputChangeDirection.none:
                next = this.currentCameraConnection?.connectionChangeDefinition[direction];
                break;
        }

        let nextInput = next ? next : this.imageConnections[0].connection.AtemImputNumber;

        this.atem.changePreview(this.AtemMixEffectBlock, nextInput);
    }

    selectedConnectionChanged(preview: number, isOnAir: boolean): void {
        if (this.currentCameraConnection !== undefined) {
            if (this.currentCameraConnection.connection.AtemImputNumber === preview) {
                return;
            }
        }

        this.currentCameraConnection = undefined;
        this.imageConnections.forEach((imageConnection) => {
            if (imageConnection.connection.AtemImputNumber === preview) {
                this.currentCameraConnection = imageConnection;
                if (isOnAir) {
                    this.pad.rumble();
                }
            }
        });
        this.printConnection(isOnAir);
    }

    printConnection(isOnAir: boolean) {
        if (this.currentCameraConnection !== undefined) {
            let additionalInfo = this.currentCameraConnection.connection.connectionAdditionalInfo();
            console.log(
                `${this.currentCameraConnection.connection.AtemImputNumber} - ${this.atem.nameGet(
                    this.currentCameraConnection.connection.AtemImputNumber
                )}${additionalInfo ? `(${additionalInfo})` : ''} ${isOnAir ? ' - onAir' : ''}`
            );
        } else {
            console.log('Input selected that is not managed with this application');
        }
    }

    mod(n: number, m: number) {
        return ((n % m) + m) % m;
    }
}
